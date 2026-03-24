#ifndef SerialCLI_h
#define SerialCLI_h

#include <Arduino.h>
#include "MetricsStore.h"

class SerialCLI {
public:
    SerialCLI(MetricsStore& store);

    void handle();
    void printHelp();

private:
    MetricsStore& _store;

    void processCommand(const String& line);
    void handleSet(const String& key, const String& value);
    void handleDelete(const String& key);
};

#endif
