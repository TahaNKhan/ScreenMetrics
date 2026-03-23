
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// Board detection and baud rate
#if defined(ESP32) || defined(ESP8266)
#define SERIAL_BAUD 115200
#else
#define SERIAL_BAUD 9600
#endif

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Settings
#define AUTO_SCROLL_INTERVAL 5000  // ms between page switches
#define INSTRUCTIONS_PRINT_INTERVAL 5000  // ms between page switches
#define METRIC_COUNT 20            // max metrics

// Metric storage
String metricKeys[METRIC_COUNT];
String metricValues[METRIC_COUNT];
int metricCount = 0;

// Display state
int currentPage = 0;
int pageCount = 0;
unsigned long lastScroll = 0;
unsigned long lastInstructionsPrint = 0;
bool needsRedraw = true;

void setup() {
  Serial.begin(SERIAL_BAUD);

  // Initialize OLED
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Initialize empty metrics
  for (int i = 0; i < METRIC_COUNT; i++) {
    metricKeys[i] = "";
    metricValues[i] = "";
  }
  
  // Add some default metrics
  setMetric("BUILD", "IDLE");
  sendUsageToSerial();
  
  updatePageCount();
  displayPage();
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

  if (millis() - lastInstructionsPrint > INSTRUCTIONS_PRINT_INTERVAL) {
    sendUsageToSerial();
    lastInstructionsPrint = millis();
  }
  
  // Auto-scroll
  if (millis() - lastScroll > AUTO_SCROLL_INTERVAL && pageCount > 1) {
    currentPage = (currentPage + 1) % pageCount;
    lastScroll = millis();
    needsRedraw = true;
  }
  
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
    Serial.println("OK: " + key + " = " + value);
    return;
  }
  
  // DEL|KEY
  if (cmd.startsWith("DEL|")) {
    String key = cmd.substring(4);
    if (delMetric(key)) {
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
    Serial.println("OK: All metrics cleared");
    return;
  }
  
  Serial.println("UNKNOWN COMMAND: " + cmd);
}

void setMetric(String key, String value) {
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

  // Page indicator at bottom
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("P");
  display.print(currentPage + 1);
  display.print("/");
  display.print(pageCount);

  display.display();
}
