#ifndef ApiServer_h
#define ApiServer_h

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "MetricsStore.h"

class ApiServer {
public:
    ApiServer(MetricsStore& store);

    void begin();
    void handle();

    void setDeviceIp(const String& ip);
    void setWifiConnected(bool connected);

private:
    ESP8266WebServer _server;
    MetricsStore& _store;

    String _ip;
    bool _wifiConnected;

    void sendHtmlDoc();
    void handleSet();
    void handleDelete();
    void handleList();
    void handleClear();
    void handleStatus();
};

#endif
