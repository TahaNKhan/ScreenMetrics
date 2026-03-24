#include "SerialCLI.h"

SerialCLI::SerialCLI(MetricsStore& store) : _store(store) {}

void SerialCLI::handle() {
    while (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            processCommand(line);
        }
    }
}

void SerialCLI::processCommand(const String& line) {
    if (line.startsWith("SET|")) {
        int sep = line.indexOf('|', 4);
        if (sep == -1) {
            Serial.println("ERROR: Use SET|KEY|VALUE");
            return;
        }
        handleSet(line.substring(4, sep), line.substring(sep + 1));
    }
    else if (line.startsWith("DEL|")) {
        handleDelete(line.substring(4));
    }
    else if (line == "LIST") {
        Serial.println(_store.list());
    }
    else if (line == "CLEAR") {
        _store.clear();
        _store.persist();
        Serial.println("OK: All metrics cleared");
    }
    else {
        Serial.println("UNKNOWN: " + line);
    }
}

void SerialCLI::handleSet(const String& key, const String& value) {
    _store.set(key, value);
    _store.persist();
    Serial.println("OK: " + key + " = " + value);
}

void SerialCLI::handleDelete(const String& key) {
    if (_store.remove(key)) {
        _store.persist();
        Serial.println("OK: Deleted " + key);
    } else {
        Serial.println("NOT FOUND: " + key);
    }
}

void SerialCLI::printHelp() {
    Serial.println("");
    Serial.println("ScreenMetrics Ready");
    Serial.println("Serial Commands: SET|KEY|VALUE | DEL|KEY | LIST | CLEAR");
    Serial.println("HTTP API: /set, /delete, /list, /clear, /status, /");
    Serial.println("mDNS: screenmetrics.local");
    Serial.println("");
}
