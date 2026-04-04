#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

class GSM {
private:
    HardwareSerial &modem;
    uint32_t baud;

    void waitForChar(char target, uint32_t timeout = 10000);

public:
    GSM(HardwareSerial &serial, uint32_t baudrate);

    void begin(int rxPin, int txPin);
    bool sendSMS(const char *phone, const char *message);

    bool readSMS(String &sender, String &content);
    // Non-blocking scanner: returns true if `needle` has been seen in modem response buffer
    bool scanFor(const char *needle);
    // Flush internal response buffer (drop accumulated modem bytes)
    void flushRX();
};

#endif
