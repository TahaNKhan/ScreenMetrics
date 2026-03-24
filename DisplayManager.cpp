#include "DisplayManager.h"

DisplayManager::DisplayManager(MetricsStore& store)
    : _display(WIDTH, HEIGHT, &Wire, RESET_PIN)
    , _store(store)
    , _currentPage(0)
    , _pageCount(1)
    , _lastScrollMs(0)
    , _wifiConnected(false)
    , needsRedraw(true)
{}

void DisplayManager::init() {
    _display.begin(SSD1306_SWITCHCAPVCC, ADDR);
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    refreshPageCount();
}

void DisplayManager::refreshPageCount() {
    _pageCount = max(1, _store.count());
    if (_currentPage >= _pageCount) _currentPage = 0;
}

void DisplayManager::requestRedraw() {
    refreshPageCount();
    needsRedraw = true;
}

void DisplayManager::setIp(const String& ip) {
    _ip = ip;
}

void DisplayManager::setWifiConnected(bool connected) {
    _wifiConnected = connected;
}

void DisplayManager::update() {
    unsigned long now = millis();

    // Auto-scroll to next page
    if (_store.count() > 1 && now - _lastScrollMs > AUTO_SCROLL_MS) {
        _currentPage = (_currentPage + 1) % _pageCount;
        _lastScrollMs = now;
        needsRedraw = true;
    }

    // Redraw if needed or if metric count changed since last draw
    static int lastKnownCount = -1;
    if (needsRedraw || _store.count() != lastKnownCount) {
        lastKnownCount = _store.count();
        refreshPageCount();
        drawMetricPage();
        needsRedraw = false;
    }
}

void DisplayManager::drawNoMetrics() {
    _display.clearDisplay();
    _display.setTextSize(TEXT_SIZE_LARGE);
    drawCenteredText("No metrics", 20, TEXT_SIZE_LARGE);
    _display.display();
}

void DisplayManager::drawCenteredText(const String& text, int y, int textSize) {
    int charWidth = textSize == TEXT_SIZE_LARGE ? 12 : 6;
    int x = (WIDTH - text.length() * charWidth) / 2;
    if (x < 0) x = 0;
    _display.setCursor(x, y);
    _display.print(text);
}

void DisplayManager::drawSeparator(int y) {
    _display.drawLine(0, y, WIDTH - 1, y, SSD1306_WHITE);
}

void DisplayManager::drawBottomBar() {
    String leftLabel = "P" + String(_currentPage + 1) + "/" + String(_pageCount);
    String rightLabel = _ip.length() > 0 ? _ip : "No IP";

    _display.setTextSize(TEXT_SIZE_SMALL);

    _display.setCursor(0, HEIGHT - 8);
    _display.print(leftLabel);

    int rightX = WIDTH - rightLabel.length() * 6;
    if (rightX < 0) rightX = 0;
    _display.setCursor(rightX, HEIGHT - 8);
    _display.print(rightLabel);
}

void DisplayManager::drawMetricPage() {
    _display.clearDisplay();

    if (_store.count() == 0) {
        drawNoMetrics();
        return;
    }

    String key = _store.keyAt(_currentPage);
    String val = _store.valueAt(_currentPage);

    if (key.length() > MAX_CHARS_PER_LINE) key = key.substring(0, MAX_CHARS_PER_LINE);
    if (val.length() > MAX_CHARS_PER_LINE) val = val.substring(0, MAX_CHARS_PER_LINE);

    _display.setTextSize(TEXT_SIZE_LARGE);
    drawCenteredText(key, 0, TEXT_SIZE_LARGE);

    drawSeparator(16);

    _display.setTextSize(TEXT_SIZE_LARGE);
    drawCenteredText(val, 20, TEXT_SIZE_LARGE);

    drawBottomBar();

    _display.display();
}
