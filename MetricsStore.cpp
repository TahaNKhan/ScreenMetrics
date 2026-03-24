#include "MetricsStore.h"

MetricsStore::MetricsStore() : _count(0) {}

void MetricsStore::init() {
    for (int i = 0; i < MAX_COUNT; i++) {
        _keys[i] = "";
        _values[i] = "";
    }
}

void MetricsStore::load() {
    int addr = 0;
    for (int i = 0; i < MAX_COUNT; i++) {
        char keyBuf[MAX_KEY_LEN + 1] = {0};
        for (int j = 0; j < MAX_KEY_LEN; j++) keyBuf[j] = EEPROM.read(addr++);
        _keys[i] = String(keyBuf);

        char valBuf[MAX_VALUE_LEN + 1] = {0};
        for (int j = 0; j < MAX_VALUE_LEN; j++) valBuf[j] = EEPROM.read(addr++);
        _values[i] = String(valBuf);

        if (_keys[i].length() > 0) _count++;
    }
}

void MetricsStore::persist() {
    int addr = 0;
    for (int i = 0; i < MAX_COUNT; i++) {
        for (int j = 0; j < MAX_KEY_LEN; j++) {
            EEPROM.write(addr++, j < _keys[i].length() ? _keys[i][j] : 0);
        }
        for (int j = 0; j < MAX_VALUE_LEN; j++) {
            EEPROM.write(addr++, j < _values[i].length() ? _values[i][j] : 0);
        }
    }
    EEPROM.commit();
}

String MetricsStore::truncate(const String& s, int maxLen) const {
    return s.length() > maxLen ? s.substring(0, maxLen) : s;
}

int MetricsStore::findByKey(const String& key) const {
    for (int i = 0; i < MAX_COUNT; i++) {
        if (_keys[i] == key) return i;
    }
    return -1;
}

int MetricsStore::findEmptySlot() const {
    for (int i = 0; i < MAX_COUNT; i++) {
        if (_keys[i] == "") return i;
    }
    return -1;
}

bool MetricsStore::set(const String& key, const String& value) {
    String k = truncate(key, MAX_KEY_LEN);
    String v = truncate(value, MAX_VALUE_LEN);

    int idx = findByKey(k);
    if (idx >= 0) {
        _values[idx] = v;
        return true;
    }

    idx = findEmptySlot();
    if (idx >= 0) {
        _keys[idx] = k;
        _values[idx] = v;
        _count++;
        return true;
    }
    return false;
}

bool MetricsStore::remove(const String& key) {
    int idx = findByKey(key);
    if (idx < 0) return false;

    _keys[idx] = "";
    _values[idx] = "";
    _count--;
    return true;
}

void MetricsStore::clear() {
    for (int i = 0; i < MAX_COUNT; i++) {
        _keys[i] = "";
        _values[i] = "";
    }
    _count = 0;
}

String MetricsStore::list() const {
    String out = "--- METRICS ---\n";
    for (int i = 0; i < MAX_COUNT; i++) {
        if (_keys[i] != "") {
            out += String(i + 1) + ". " + _keys[i] + " = " + _values[i] + "\n";
        }
    }
    out += "---------------";
    return out;
}

String MetricsStore::status(const String& ip) const {
    String out = "IP: " + ip + "\n";
    out += "Metrics: " + String(_count) + "/" + String(MAX_COUNT) + "\n";
    out += "EEPROM: " + String(EEPROM_SIZE) + " bytes reserved\n";
    return out;
}

String MetricsStore::keyAt(int index) const {
    if (index < 0 || index >= MAX_COUNT) return "";
    return _keys[index];
}

String MetricsStore::valueAt(int index) const {
    if (index < 0 || index >= MAX_COUNT) return "";
    return _values[index];
}
