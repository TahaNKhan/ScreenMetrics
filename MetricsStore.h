#ifndef MetricsStore_h
#define MetricsStore_h

#include <EEPROM.h>

class MetricsStore {
public:
    static const int MAX_COUNT = 20;
    static const int MAX_KEY_LEN = 16;
    static const int MAX_VALUE_LEN = 16;
    static const int EEPROM_SIZE = 1024;

    MetricsStore();

    void init();
    void load();
    void persist();

    bool set(const String& key, const String& value);
    bool remove(const String& key);
    void clear();
    String list() const;
    String status(const String& ip) const;

    int count() const { return _count; }
    int maxCount() const { return MAX_COUNT; }

    String keyAt(int index) const;
    String valueAt(int index) const;

private:
    String _keys[MAX_COUNT];
    String _values[MAX_COUNT];
    int _count;

    int findByKey(const String& key) const;
    int findEmptySlot() const;
    String truncate(const String& s, int maxLen) const;
};

#endif
