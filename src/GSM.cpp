
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

// --- Gửi SMS ---
void GSM::sendSMS(const char *phone, const char *message)
{
    modem.println("AT+CMGF=1");
    delay(1000);

    modem.print("AT+CMGS=\"");
    modem.print(phone);
    modem.println("\"");

    // chờ dấu >
    waitForChar('>');

    modem.print(message);
    delay(200);
    modem.write(26); // Ctrl+Z
}
