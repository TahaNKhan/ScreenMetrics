#include "ApiServer.h"

ApiServer::ApiServer(MetricsStore& store)
    : _server(80)
    , _store(store)
    , _wifiConnected(false)
{}

void ApiServer::setDeviceIp(const String& ip) {
    _ip = ip;
}

void ApiServer::setWifiConnected(bool connected) {
    _wifiConnected = connected;
}

void ApiServer::begin() {
    _server.on("/", [this]() { sendHtmlDoc(); });
    _server.on("/set", [this]() { handleSet(); });
    _server.on("/delete", [this]() { handleDelete(); });
    _server.on("/list", [this]() { handleList(); });
    _server.on("/clear", [this]() { handleClear(); });
    _server.on("/status", [this]() { handleStatus(); });
    _server.begin();
}

void ApiServer::handle() {
    _server.handleClient();
}

void ApiServer::sendHtmlDoc() {
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
        "<strong>Device IP:</strong> " + _ip + "<br>"
        "<strong>mDNS:</strong> screenmetrics.local<br>"
        "<strong>Status:</strong> " + String(_wifiConnected ? "Connected to WiFi" : "Not connected") +
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
        "<div class='endpoint'><h3>Set a metric</h3><pre>curl \"http://" + (_ip.length() ? _ip : "YOUR_DEVICE_IP") + "/set?key=BUILD&amp;value=PASSING\"</pre></div>"
        "<div class='endpoint'><h3>Delete a metric</h3><pre>curl \"http://" + (_ip.length() ? _ip : "YOUR_DEVICE_IP") + "/delete?key=BUILD\"</pre></div>"
        "<div class='endpoint'><h3>List all metrics</h3><pre>curl http://" + (_ip.length() ? _ip : "YOUR_DEVICE_IP") + "/list</pre></div>"
        "<footer>ScreenMetrics &mdash; ESP8266 OLED Display</footer>"
        "</body></html>";
    _server.send(200, "text/html", html);
}

void ApiServer::handleSet() {
    String key = _server.arg("key");
    String value = _server.arg("value");

    if (key.length() == 0 || value.length() == 0) {
        _server.send(400, "text/plain", "ERROR: missing key or value");
        return;
    }

    _store.set(key, value);
    _store.persist();
    _server.send(200, "text/plain", "OK: " + key + " = " + value);
}

void ApiServer::handleDelete() {
    String key = _server.arg("key");

    if (key.length() == 0) {
        _server.send(400, "text/plain", "ERROR: missing key");
        return;
    }

    if (_store.remove(key)) {
        _store.persist();
        _server.send(200, "text/plain", "OK: Deleted " + key);
    } else {
        _server.send(404, "text/plain", "NOT FOUND: " + key);
    }
}

void ApiServer::handleList() {
    _server.send(200, "text/plain", _store.list());
}

void ApiServer::handleClear() {
    _store.clear();
    _store.persist();
    _server.send(200, "text/plain", "OK: All metrics cleared");
}

void ApiServer::handleStatus() {
    _server.send(200, "text/plain", _store.status(_ip));
}
