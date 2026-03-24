#ifndef DisplayManager_h
#define DisplayManager_h

#include <Adafruit_SSD1306.h>
#include "MetricsStore.h"

class DisplayManager {
public:
    static const int WIDTH = 128;
    static const int HEIGHT = 64;
    static const int ADDR = 0x3C;
    static const int RESET_PIN = -1;

    static const int AUTO_SCROLL_MS = 5000;
    static const int TEXT_SIZE_LARGE = 2;
    static const int TEXT_SIZE_SMALL = 1;
    static const int MAX_CHARS_PER_LINE = 10;

    DisplayManager(MetricsStore& store);

    void init();
    void update();
    void requestRedraw();

    void setIp(const String& ip);
    void setWifiConnected(bool connected);

private:
    Adafruit_SSD1306 _display;
    MetricsStore& _store;

    String _ip;
    bool _wifiConnected;

    int _currentPage;
    int _pageCount;
    unsigned long _lastScrollMs;

    void drawMetricPage();
    void drawNoMetrics();
    void drawSeparator(int y);
    void drawCenteredText(const String& text, int y, int textSize);
    void drawBottomBar();
    void refreshPageCount();
};

#endif
