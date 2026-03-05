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
};

#endif
