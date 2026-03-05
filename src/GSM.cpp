#include "GSM.h"

// --- Constructor ---
GSM::GSM(HardwareSerial &serial, uint32_t baudrate)
    : modem(serial), baud(baudrate) {}

// --- Khởi động modem ---
void GSM::begin(int rxPin, int txPin)
{
    modem.begin(baud, SERIAL_8N1, rxPin, txPin);
    delay(12000); // chờ modem ổn định
}

// --- Chờ ký tự phản hồi từ modem ---
void GSM::waitForChar(char target, uint32_t timeout)
{
    uint32_t start = millis();
    while (millis() - start < timeout)
    {
        if (modem.available())
        {
            char c = modem.read();
            Serial.write(c); // log debug
            if (c == target) return;
        }
    }
}

// Wait for a substring in modem response; returns true if found before timeout
static bool waitForResponse(HardwareSerial &modemRef, const char *needle, uint32_t timeout)
{
    uint32_t start = millis();
    String buf;
    while (millis() - start < timeout)
    {
        while (modemRef.available()) {
            char c = modemRef.read();
            buf += c;
            // keep buffer bounded
            if (buf.length() > 512) buf = buf.substring(buf.length() - 512);
        }
        if (buf.indexOf(needle) >= 0) return true;
        delay(10);
    }
    return false;
}

// --- Gửi SMS ---
bool GSM::sendSMS(const char *phone, const char *message)
{
    // Set text mode
    modem.println("AT+CMGF=1");
    if (!waitForResponse(modem, "OK", 2000)) {
        Serial.println("[GSM] no OK after CMGF");
        // continue attempt, but report failure
    }

    // Start send
    modem.print("AT+CMGS=\"");
    modem.print(phone);
    modem.println("\"");

    // wait for '>' prompt
    if (!waitForResponse(modem, ">", 5000)) {
        Serial.println("[GSM] no '>' prompt for CMGS");
        return false;
    }

    modem.print(message);
    delay(200);
    modem.write(26); // Ctrl+Z

    // After sending, modem replies with +CMGS: <mr> and then OK, or ERROR
    if (waitForResponse(modem, "+CMGS", 15000) || waitForResponse(modem, "OK", 15000)) {
        return true;
    }

    // final check for ERROR
    if (waitForResponse(modem, "ERROR", 1000)) {
        Serial.println("[GSM] CMGS ERROR");
        return false;
    }

    // Default to failure if no positive response
    return false;
}
