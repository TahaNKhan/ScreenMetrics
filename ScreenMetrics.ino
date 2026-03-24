/*
 * ScreenMetrics - ESP8266 OLED Metrics Display
 * 
 * A WiFi-connected device that displays metrics on an OLED screen.
 * Metrics are persisted to EEPROM and accessible via HTTP API.
 * 
 * HTTP API:
 *   GET /              - API documentation (HTML)
 *   GET /set?key=&value= - Set/update a metric
 *   GET /delete?key=     - Remove a metric by key
 *   GET /list          - List all metrics
 *   GET /clear         - Remove all metrics
 *   GET /status        - Device status
 */

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define SERIAL_BAUD           115200
#define SCREEN_WIDTH          128
#define SCREEN_HEIGHT         64
#define OLED_ADDR             0x3C
#define OLED_RESET            -1

#define METRIC_COUNT          20
#define MAX_KEY_LEN           16
#define MAX_VALUE_LEN         16
#define EEPROM_SIZE           1024

#define AUTO_SCROLL_MS        5000
#define TEXT_SIZE_KEY         2
#define TEXT_SIZE_VALUE       2
#define TEXT_SIZE_LABEL       1
#define MAX_CHARS_PER_LINE    10

#define AP_SSID               "ScreenMetrics"
#define AP_PASSWORD           "12345678"

// ============================================================================
// GLOBAL STATE
// ============================================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP8266WebServer server(80);

// --- Display ---
int currentPage = 0;
int pageCount = 1;
unsigned long lastScrollMs = 0;
bool needsRedraw = false;

// --- Metrics Storage ---
String metricKeys[METRIC_COUNT];
String metricValues[METRIC_COUNT];
int metricCount = 0;

// --- Network ---
String deviceIP = "";
bool wifiConnected = false;

// ============================================================================
// DISPLAY HELPERS
// ============================================================================

void clearDisplay() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
}

void drawCenteredText(const String& text, int y, int textSize) {
    int charWidth = textSize == TEXT_SIZE_KEY ? 12 : 6;
    int x = (SCREEN_WIDTH - text.length() * charWidth) / 2;
    if (x < 0) x = 0;
    display.setCursor(x, y);
    display.print(text);
}

void drawSeparator(int y) {
    display.drawLine(0, y, SCREEN_WIDTH - 1, y, SSD1306_WHITE);
}

void drawBottomBar(const String& leftText, const String& rightText) {
    display.setTextSize(TEXT_SIZE_LABEL);
    display.setCursor(0, SCREEN_HEIGHT - 8);
    display.print(leftText);
    
    int rightX = SCREEN_WIDTH - rightText.length() * 6;
    if (rightX < 0) rightX = 0;
    display.setCursor(rightX, SCREEN_HEIGHT - 8);
    display.print(rightText);
}

void drawMetricPage() {
    clearDisplay();
    
    if (currentPage >= metricCount || metricKeys[currentPage] == "") {
        display.setTextSize(TEXT_SIZE_KEY);
        drawCenteredText("No metrics", 20, TEXT_SIZE_KEY);
        display.display();
        return;
    }
    
    String key = truncate(metricKeys[currentPage], MAX_CHARS_PER_LINE);
    String val = truncate(metricValues[currentPage], MAX_CHARS_PER_LINE);
    
    display.setTextSize(TEXT_SIZE_KEY);
    drawCenteredText(key, 0, TEXT_SIZE_KEY);
    drawSeparator(16);
    
    display.setTextSize(TEXT_SIZE_VALUE);
    drawCenteredText(val, 20, TEXT_SIZE_VALUE);
    
    String pageLabel = "P" + String(currentPage + 1) + "/" + String(pageCount);
    drawBottomBar(pageLabel, deviceIP);
    
    display.display();
}

// ============================================================================
// STRING HELPERS
// ============================================================================

String truncate(const String& s, int maxLen) {
    return s.length() > maxLen ? s.substring(0, maxLen) : s;
}

String buildMetricList() {
    String out = "--- METRICS ---\n";
    for (int i = 0; i < METRIC_COUNT; i++) {
        if (metricKeys[i] != "") {
            out += String(i + 1) + ". " + metricKeys[i] + " = " + metricValues[i] + "\n";
        }
    }
    out += "---------------";
    return out;
}

// ============================================================================
// METRIC OPERATIONS
// ============================================================================

void initMetrics() {
    for (int i = 0; i < METRIC_COUNT; i++) {
        metricKeys[i] = "";
        metricValues[i] = "";
    }
}

int findMetric(const String& key) {
    for (int i = 0; i < METRIC_COUNT; i++) {
        if (metricKeys[i] == key) return i;
    }
    return -1;
}

int findEmptySlot() {
    for (int i = 0; i < METRIC_COUNT; i++) {
        if (metricKeys[i] == "") return i;
    }
    return -1;
}

void upsertMetric(const String& key, const String& value) {
    String k = truncate(key, MAX_KEY_LEN);
    String v = truncate(value, MAX_VALUE_LEN);
    
    int idx = findMetric(k);
    if (idx >= 0) {
        metricValues[idx] = v;
    } else {
        idx = findEmptySlot();
        if (idx >= 0) {
            metricKeys[idx] = k;
            metricValues[idx] = v;
            metricCount++;
        }
    }
    needsRedraw = true;
}

bool removeMetric(const String& key) {
    int idx = findMetric(key);
    if (idx < 0) return false;
    
    metricKeys[idx] = "";
    metricValues[idx] = "";
    metricCount--;
    needsRedraw = true;
    return true;
}

void clearAllMetrics() {
    for (int i = 0; i < METRIC_COUNT; i++) {
        metricKeys[i] = "";
        metricValues[i] = "";
    }
    metricCount = 0;
    needsRedraw = true;
}

void refreshPageCount() {
    pageCount = max(1, metricCount);
    if (currentPage >= pageCount) currentPage = 0;
}

// ============================================================================
// PERSISTENCE (EEPROM)
// ============================================================================

void persistMetrics() {
    int addr = 0;
    for (int i = 0; i < METRIC_COUNT; i++) {
        for (int j = 0; j < MAX_KEY_LEN; j++) {
            EEPROM.write(addr++, j < metricKeys[i].length() ? metricKeys[i][j] : 0);
        }
        for (int j = 0; j < MAX_VALUE_LEN; j++) {
            EEPROM.write(addr++, j < metricValues[i].length() ? metricValues[i][j] : 0);
        }
    }
    EEPROM.commit();
}

void loadMetricsFromEeprom() {
    int addr = 0;
    for (int i = 0; i < METRIC_COUNT; i++) {
        char keyBuf[MAX_KEY_LEN + 1] = {0};
        for (int j = 0; j < MAX_KEY_LEN; j++) keyBuf[j] = EEPROM.read(addr++);
        
        char valBuf[MAX_VALUE_LEN + 1] = {0};
        for (int j = 0; j < MAX_VALUE_LEN; j++) valBuf[j] = EEPROM.read(addr++);
        
        metricKeys[i] = String(keyBuf);
        metricValues[i] = String(valBuf);
        if (metricKeys[i].length() > 0) metricCount++;
    }
    refreshPageCount();
}

// ============================================================================
// HTTP API
// ============================================================================

void sendHtmlDoc() {
    String html = "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>ScreenMetrics API</title>"
        "<style>"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;max-width:800px;margin:0 auto;padding:20px;background:#f5f5f5;color:#333}"
        "h1{color:#222;border-bottom:2px solid #ddd;padding-bottom:10px}"
        "h2{color:#555;margin-top:30px}"
        "code{background:#e8e8e8;padding:2px 6px;border-radius:3px;font-size:14px}"
        "pre{background:#333;color:#fff;padding:15px;border-radius:5px;overflow-x:auto}"
        ".endpoint{margin:15px 0;padding:15px;background:#fff;border-radius:5px;box-shadow:0 1px 3px rgba(0,0,0,0.1)}"
        ".method{display:inline-block;padding:4px 10px;border-radius:4px;font-weight:bold;margin-right:10px}"
        ".get{background:#61affe;color:#fff}.delete{background:#f93e3e;color:#fff}"
        ".info{background:#d1ecf1;border:1px solid #bee5eb;padding:15px;border-radius:5px;margin:20px 0}"
        "table{width:100%;border-collapse:collapse;margin:15px 0}"
        "th,td{padding:12px;text-align:left;border-bottom:1px solid #ddd}"
        "th{background:#f8f8f8;font-weight:600}"
        "footer{text-align:center;color:#888;font-size:12px;margin-top:40px}"
        "</style></head><body>"
        "<h1>ScreenMetrics API</h1>"
        "<div class='info'>"
        "<strong>Device IP:</strong> " + deviceIP + "<br>"
        "<strong>mDNS:</strong> screenmetrics.local<br>"
        "<strong>Status:</strong> " + String(wifiConnected ? "Connected to WiFi" : "Not connected") +
        "</div>"
        "<h2>Endpoints</h2>"
        "<table>"
        "<tr><th>Method</th><th>Endpoint</th><th>Description</th></tr>"
        "<tr><td><span class='method get'>GET</span></td><td><code>/</code></td><td>This documentation page</td></tr>"
        "<tr><td><span class='method get'>GET</span></td><td><code>/set?key=NAME&amp;value=VALUE</code></td><td>Set or update a metric</td></tr>"
        "<tr><td><span class='method delete'>GET</span></td><td><code>/delete?key=NAME</code></td><td>Remove a metric by key</td></tr>"
        "<tr><td><span class='method get'>GET</span></td><td><code>/list</code></td><td>List all metrics</td></tr>"
        "<tr><td><span class='method delete'>GET</span></td><td><code>/clear</code></td><td>Remove all metrics</td></tr>"
        "<tr><td><span class='method get'>GET</span></td><td><code>/status</code></td><td>Device status</td></tr>"
        "</table>"
        "<h2>Examples</h2>"
        "<div class='endpoint'><h3>Set a metric</h3><pre>curl \"http://screenmetrics.local/set?key=BUILD&amp;value=PASSING\"</pre></div>"
        "<div class='endpoint'><h3>Delete a metric</h3><pre>curl \"http://screenmetrics.local/delete?key=BUILD\"</pre></div>"
        "<div class='endpoint'><h3>List all metrics</h3><pre>curl http://screenmetrics.local/list</pre></div>"
        "<footer>ScreenMetrics &mdash; ESP8266 OLED Display</footer>"
        "</body></html>";
    server.send(200, "text/html", html);
}

void handleApiSet() {
    String key = server.arg("key");
    String value = server.arg("value");
    
    if (key.length() == 0 || value.length() == 0) {
        server.send(400, "text/plain", "ERROR: missing key or value");
        return;
    }
    
    upsertMetric(key, value);
    persistMetrics();
    server.send(200, "text/plain", "OK: " + key + " = " + value);
}

void handleApiDelete() {
    String key = server.arg("key");
    
    if (key.length() == 0) {
        server.send(400, "text/plain", "ERROR: missing key");
        return;
    }
    
    if (removeMetric(key)) {
        persistMetrics();
        refreshPageCount();
        server.send(200, "text/plain", "OK: Deleted " + key);
    } else {
        server.send(404, "text/plain", "NOT FOUND: " + key);
    }
}

void handleApiList() {
    server.send(200, "text/plain", buildMetricList());
}

void handleApiClear() {
    clearAllMetrics();
    persistMetrics();
    refreshPageCount();
    server.send(200, "text/plain", "OK: All metrics cleared");
}

void handleApiStatus() {
    String out = "IP: " + deviceIP + "\n";
    out += "Metrics: " + String(metricCount) + "/" + String(METRIC_COUNT) + "\n";
    out += "EEPROM: " + String(EEPROM_SIZE) + " bytes reserved\n";
    server.send(200, "text/plain", out);
}

void setupApiServer() {
    server.on("/", sendHtmlDoc);
    server.on("/set", handleApiSet);
    server.on("/delete", handleApiDelete);
    server.on("/list", handleApiList);
    server.on("/clear", handleApiClear);
    server.on("/status", handleApiStatus);
    server.begin();
}

// ============================================================================
// SERIAL COMMANDS
// ============================================================================

void handleSerialSet(const String& key, const String& value) {
    upsertMetric(key, value);
    persistMetrics();
    Serial.println("OK: " + key + " = " + value);
}

void handleSerialDelete(const String& key) {
    if (removeMetric(key)) {
        persistMetrics();
        refreshPageCount();
        Serial.println("OK: Deleted " + key);
    } else {
        Serial.println("NOT FOUND: " + key);
    }
}

void handleSerialList() {
    Serial.println(buildMetricList());
}

void handleSerialClear() {
    clearAllMetrics();
    persistMetrics();
    refreshPageCount();
    Serial.println("OK: All metrics cleared");
}

void processSerialCommand(const String& line) {
    if (line.startsWith("SET|")) {
        int sep = line.indexOf('|', 4);
        if (sep == -1) { Serial.println("ERROR: Use SET|KEY|VALUE"); return; }
        handleSerialSet(line.substring(4, sep), line.substring(sep + 1));
    }
    else if (line.startsWith("DEL|")) {
        handleSerialDelete(line.substring(4));
    }
    else if (line == "LIST") {
        handleSerialList();
    }
    else if (line == "CLEAR") {
        handleSerialClear();
    }
    else {
        Serial.println("UNKNOWN: " + line);
    }
}

void printSerialHelp() {
    Serial.println("");
    Serial.println("ScreenMetrics Ready");
    Serial.println("Serial Commands: SET|KEY|VALUE | DEL|KEY | LIST | CLEAR");
    Serial.println("HTTP API: /set, /delete, /list, /clear, /status, /");
    Serial.println("mDNS: screenmetrics.local");
    Serial.println("");
}

// ============================================================================
// WIFI
// ============================================================================

void connectWifi() {
    display.clearDisplay();
    display.setTextSize(TEXT_SIZE_LABEL);
    display.setCursor(0, 20);
    display.print("Connecting WiFi...");
    display.display();
    
    WiFiManager wm;
    wm.autoConnect(AP_SSID, AP_PASSWORD);
    
    wifiConnected = true;
    deviceIP = WiFi.localIP().toString();
    
    Serial.println("WiFi Connected: " + WiFi.SSID());
    Serial.println("IP: " + deviceIP);
}

// ============================================================================
// SETUP & LOOP
// ============================================================================

void setup() {
    Serial.begin(SERIAL_BAUD);
    
    EEPROM.begin(EEPROM_SIZE);
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    clearDisplay();
    
    initMetrics();
    loadMetricsFromEeprom();
    
    if (metricCount == 0) {
        upsertMetric("BUILD", "IDLE");
        persistMetrics();
    }
    
    connectWifi();
    setupApiServer();
    
    if (MDNS.begin("screenmetrics")) {
        MDNS.addService("http", "tcp", 80);
    }
    
    drawMetricPage();
    printSerialHelp();
}

void loop() {
    // Serial input
    while (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) processSerialCommand(line);
    }
    
    // Auto-scroll
    if (millis() - lastScrollMs > AUTO_SCROLL_MS && pageCount > 1) {
        currentPage = (currentPage + 1) % pageCount;
        lastScrollMs = millis();
        needsRedraw = true;
    }
    
    // HTTP requests
    server.handleClient();
    
    // Display update
    if (needsRedraw) {
        drawMetricPage();
        needsRedraw = false;
    }
}
