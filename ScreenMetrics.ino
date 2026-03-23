
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define SERIAL_BAUD 115200

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Settings
#define AUTO_SCROLL_INTERVAL 5000  // ms between page switches
#define METRIC_COUNT 20            // max metrics
#define MAX_KEY_LEN 16
#define MAX_VALUE_LEN 16
#define EEPROM_SIZE 1024           // bytes reserved for EEPROM
#define METRIC_SLOT_SIZE 34        // 16 key + 1 separator + 16 value + 1 null = 34

// Metric storage
String metricKeys[METRIC_COUNT];
String metricValues[METRIC_COUNT];
int metricCount = 0;

// Display state
int currentPage = 0;
int pageCount = 0;
unsigned long lastScroll = 0;
bool needsRedraw = true;

// WiFi/Web server state
String deviceIP = "";
bool wifiConnected = false;
ESP8266WebServer server(80);

void setup() {
  Serial.begin(SERIAL_BAUD);

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Initialize OLED
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Initialize empty metrics
  for (int i = 0; i < METRIC_COUNT; i++) {
    metricKeys[i] = "";
    metricValues[i] = "";
  }

  // Load saved metrics from EEPROM
  loadMetrics();

  // If no saved metrics, add default
  if (metricCount == 0) {
    setMetric("BUILD", "IDLE");
    saveMetrics();
  }

  // Connect to WiFi
  connectWiFi();

  // Setup web server
  setupWebServer();

  // Setup mDNS
  if (MDNS.begin("screenmetrics")) {
    MDNS.addService("http", "tcp", 80);
  }

  updatePageCount();
  displayPage();
  sendUsageToSerial();
}

void connectWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Connecting WiFi...");
  display.display();

  WiFiManager wifiManager;
  // Uncomment to reset saved credentials:
  // wifiManager.resetSettings();

  // Auto-connect, creates AP "ScreenMetrics" if failed
  wifiManager.autoConnect("ScreenMetrics", "12345678");

  wifiConnected = true;
  deviceIP = WiFi.localIP().toString();

  Serial.println("");
  Serial.println("WiFi Connected: " + WiFi.SSID());
  Serial.println("IP: " + deviceIP);
  Serial.println("mDNS: screenmetrics.local");

  needsRedraw = true;
}

void setupWebServer() {
  server.on("/set", []() {
    String key = server.arg("key");
    String value = server.arg("value");
    if (key.length() > 0 && value.length() > 0) {
      setMetric(key, value);
      saveMetrics();  // Persist to EEPROM
      server.send(200, "text/plain", "OK: " + key + " = " + value);
    } else {
      server.send(400, "text/plain", "ERROR: missing key or value");
    }
  });

  server.on("/list", []() {
    String response = "--- METRICS ---\n";
    for (int i = 0; i < METRIC_COUNT; i++) {
      if (metricKeys[i] != "") {
        response += String(i + 1) + ". " + metricKeys[i] + " = " + metricValues[i] + "\n";
      }
    }
    response += "---------------";
    server.send(200, "text/plain", response);
  });

  server.on("/clear", []() {
    for (int i = 0; i < METRIC_COUNT; i++) {
      metricKeys[i] = "";
      metricValues[i] = "";
    }
    metricCount = 0;
    updatePageCount();
    saveMetrics();  // Persist to EEPROM
    server.send(200, "text/plain", "OK: All metrics cleared");
  });

  server.on("/status", []() {
    String response = "IP: " + deviceIP + "\n";
    response += "Metrics: " + String(metricCount) + "/" + String(METRIC_COUNT) + "\n";
    response += "EEPROM: " + String(EEPROM_SIZE) + " bytes reserved\n";
    server.send(200, "text/plain", response);
  });

  server.begin();
}

void sendUsageToSerial() {
  Serial.println("");
  Serial.println("OLED Metrics Display Ready");
  Serial.println("Commands:");
  Serial.println("  SET|KEY|VALUE   - Set/overwrite metric");
  Serial.println("  DEL|KEY         - Delete metric");
  Serial.println("  LIST            - Show all metrics");
  Serial.println("  CLEAR           - Clear all metrics");
  Serial.println("");
  Serial.println("HTTP API:");
  Serial.println("  GET /set?key=NAME&value=VALUE");
  Serial.println("  GET /list");
  Serial.println("  GET /clear");
  Serial.println("  GET /status");
  Serial.println("");
  Serial.println("mDNS: screenmetrics.local");
  Serial.println("");
  Serial.println("Metrics are persisted to EEPROM.");
  Serial.println("");
}

void loop() {
  // Handle incoming serial data
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      processCommand(line);
    }
  }

  // Auto-scroll
  if (millis() - lastScroll > AUTO_SCROLL_INTERVAL && pageCount > 1) {
    currentPage = (currentPage + 1) % pageCount;
    lastScroll = millis();
    needsRedraw = true;
  }

  // Handle web requests
  server.handleClient();

  // Redraw if needed
  if (needsRedraw) {
    displayPage();
    needsRedraw = false;
  }
}

void processCommand(String cmd) {
  cmd.trim();

  // SET|KEY|VALUE
  if (cmd.startsWith("SET|")) {
    int p1 = cmd.indexOf('|', 4);
    if (p1 == -1) {
      Serial.println("ERROR: Use SET|KEY|VALUE");
      return;
    }
    String key = cmd.substring(4, p1);
    String value = cmd.substring(p1 + 1);
    setMetric(key, value);
    saveMetrics();  // Persist to EEPROM
    Serial.println("OK: " + key + " = " + value);
    return;
  }

  // DEL|KEY
  if (cmd.startsWith("DEL|")) {
    String key = cmd.substring(4);
    if (delMetric(key)) {
      saveMetrics();  // Persist to EEPROM
      Serial.println("OK: Deleted " + key);
    } else {
      Serial.println("NOT FOUND: " + key);
    }
    return;
  }

  // LIST
  if (cmd == "LIST") {
    Serial.println("--- METRICS ---");
    for (int i = 0; i < METRIC_COUNT; i++) {
      if (metricKeys[i] != "") {
        Serial.println(String(i + 1) + ". " + metricKeys[i] + " = " + metricValues[i]);
      }
    }
    Serial.println("---------------");
    return;
  }

  // CLEAR
  if (cmd == "CLEAR") {
    for (int i = 0; i < METRIC_COUNT; i++) {
      metricKeys[i] = "";
      metricValues[i] = "";
    }
    metricCount = 0;
    updatePageCount();
    saveMetrics();  // Persist to EEPROM
    Serial.println("OK: All metrics cleared");
    return;
  }

  Serial.println("UNKNOWN COMMAND: " + cmd);
}

void setMetric(String key, String value) {
  // Truncate key and value to fit
  if (key.length() > MAX_KEY_LEN) key = key.substring(0, MAX_KEY_LEN);
  if (value.length() > MAX_VALUE_LEN) value = value.substring(0, MAX_VALUE_LEN);

  // Check if exists, overwrite
  for (int i = 0; i < METRIC_COUNT; i++) {
    if (metricKeys[i] == key) {
      metricValues[i] = value;
      needsRedraw = true;
      return;
    }
  }

  // Add new
  for (int i = 0; i < METRIC_COUNT; i++) {
    if (metricKeys[i] == "") {
      metricKeys[i] = key;
      metricValues[i] = value;
      metricCount++;
      updatePageCount();
      needsRedraw = true;
      return;
    }
  }

  Serial.println("WARN: Metric full, delete some first");
}

bool delMetric(String key) {
  for (int i = 0; i < METRIC_COUNT; i++) {
    if (metricKeys[i] == key) {
      metricKeys[i] = "";
      metricValues[i] = "";
      metricCount--;
      updatePageCount();
      needsRedraw = true;
      return true;
    }
  }
  return false;
}

void saveMetrics() {
  int addr = 0;
  for (int i = 0; i < METRIC_COUNT; i++) {
    // Write key (up to MAX_KEY_LEN chars)
    for (int j = 0; j < MAX_KEY_LEN; j++) {
      EEPROM.write(addr++, j < metricKeys[i].length() ? metricKeys[i][j] : 0);
    }
    // Write value (up to MAX_VALUE_LEN chars)
    for (int j = 0; j < MAX_VALUE_LEN; j++) {
      EEPROM.write(addr++, j < metricValues[i].length() ? metricValues[i][j] : 0);
    }
  }
  EEPROM.commit();
}

void loadMetrics() {
  int addr = 0;
  for (int i = 0; i < METRIC_COUNT; i++) {
    // Read key
    char keyBuf[MAX_KEY_LEN + 1] = {0};
    for (int j = 0; j < MAX_KEY_LEN; j++) {
      keyBuf[j] = EEPROM.read(addr++);
    }
    // Read value
    char valBuf[MAX_VALUE_LEN + 1] = {0};
    for (int j = 0; j < MAX_VALUE_LEN; j++) {
      valBuf[j] = EEPROM.read(addr++);
    }

    metricKeys[i] = String(keyBuf);
    metricValues[i] = String(valBuf);

    // Count non-empty metrics
    if (metricKeys[i].length() > 0) {
      metricCount++;
    }
  }
  updatePageCount();
}

void updatePageCount() {
  pageCount = metricCount;
  if (pageCount == 0) pageCount = 1;
  if (currentPage >= pageCount) currentPage = 0;
}

void displayPage() {
  display.clearDisplay();

  // Get current metric
  if (currentPage >= metricCount || metricKeys[currentPage] == "") {
    display.setTextSize(2);
    int x = (128 - 9 * 12) / 2;  // "No metrics" = 9 chars
    if (x < 0) x = 0;
    display.setCursor(x, 20);
    display.print("No metrics");
    display.display();
    return;
  }

  String key = metricKeys[currentPage];
  String val = metricValues[currentPage];

  // Key - size 2 at top, centered
  display.setTextSize(2);
  // Truncate key if too long (width ~10 chars at size 2)
  if (key.length() > 10) key = key.substring(0, 10);
  int keyX = (128 - key.length() * 12) / 2;
  if (keyX < 0) keyX = 0;
  display.setCursor(keyX, 0);
  display.print(key);

  // Draw line separator
  display.drawLine(0, 16, 127, 16, SSD1306_WHITE);

  // Value - size 2, below the line
  int charWidth = 12;
  int maxChars = 10;
  if (val.length() > maxChars) val = val.substring(0, maxChars);
  int valX = (128 - val.length() * charWidth) / 2;
  if (valX < 0) valX = 0;
  display.setCursor(valX, 20);
  display.print(val);

  // Page indicator at bottom left
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("P");
  display.print(currentPage + 1);
  display.print("/");
  display.print(pageCount);

  // IP address at bottom right
  if (wifiConnected && deviceIP.length() > 0) {
    int ipX = 128 - deviceIP.length() * 6;  // size 1 char width ~6
    if (ipX < 0) ipX = 0;
    display.setCursor(ipX, 56);
    display.print(deviceIP);
  }

  display.display();
}
