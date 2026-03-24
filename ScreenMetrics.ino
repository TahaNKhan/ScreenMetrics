/*
 * ScreenMetrics - ESP8266 OLED Metrics Display
 * 
 * Architecture:
 *   MetricsStore  - metric CRUD + EEPROM persistence
 *   DisplayManager - OLED rendering + auto-scroll
 *   ApiServer     - HTTP REST API
 *   SerialCLI     - serial command interface
 */

#include <Wire.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

#include "MetricsStore.h"
#include "DisplayManager.h"
#include "ApiServer.h"
#include "SerialCLI.h"

// ============================================================================
// CONSTANTS
// ============================================================================

#define SERIAL_BAUD     115200
#define AP_SSID         "ScreenMetrics"
#define AP_PASSWORD     "12345678"

// ============================================================================
// COMPONENTS
// ============================================================================

MetricsStore metrics;
DisplayManager display(metrics);
ApiServer api(metrics);
SerialCLI serial(metrics);

// ============================================================================
// WIFI
// ============================================================================

void connectWifi() {
    display.setIp("Connecting...");

    WiFiManager wm;
    wm.autoConnect(AP_SSID, AP_PASSWORD);

    String ip = WiFi.localIP().toString();
    display.setIp(ip);
    display.setWifiConnected(true);
    api.setDeviceIp(ip);
    api.setWifiConnected(true);

    Serial.println("WiFi Connected: " + WiFi.SSID());
    Serial.println("IP: " + ip);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD);
    EEPROM.begin(MetricsStore::EEPROM_SIZE);

    metrics.init();
    metrics.load();
    display.init();

    if (metrics.count() == 0) {
        metrics.set("BUILD", "IDLE");
        metrics.persist();
    }

    connectWifi();
    api.begin();

    if (MDNS.begin("screenmetrics")) {
        MDNS.addService("http", "tcp", 80);
    }

    serial.printHelp();
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    serial.handle();
    api.handle();
    display.update();
}
