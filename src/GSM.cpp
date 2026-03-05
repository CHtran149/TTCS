#include "GSM.h"

// --- Constructor ---
GSM::GSM(HardwareSerial &serial, uint32_t baudrate)
    : modem(serial), baud(baudrate) {}

// --- Khởi động modem ---
bool GSM::begin(int rxPin, int txPin)
{
    modem.begin(baud, SERIAL_8N1, rxPin, txPin);
    delay(2000); // chờ modem khởi động cơ bản

    modem.println("AT");
    return waitForResponse("OK", 5000);
}

// --- Chờ phản hồi chuỗi ---
bool GSM::waitForResponse(const char *target, uint32_t timeout)
{
    uint32_t start = millis();
    String buffer;
    while (millis() - start < timeout) {
        while (modem.available()) {
            char c = modem.read();
            buffer += c;
            Serial.write(c); // log debug
            if (buffer.indexOf(target) != -1) {
                return true;
            }
        }
    }
    return false;
}

// --- Gửi SMS với retry ---
bool GSM::sendSMS(const char *phone, const char *message, int retries)
{
    for (int attempt = 0; attempt < retries; attempt++) {
        modem.println("AT+CMGF=1");
        if (!waitForResponse("OK", 2000)) continue;

        modem.print("AT+CMGS=\"");
        modem.print(phone);
        modem.println("\"");

        if (!waitForResponse(">", 5000)) continue;

        modem.print(message);
        delay(200);
        modem.write(26); // Ctrl+Z

        if (waitForResponse("OK", 10000)) {
            return true; // gửi thành công
        }
    }
    return false; // thất bại sau khi retry
}

// --- Kiểm tra modem đã đăng ký mạng chưa ---
bool GSM::isRegistered()
{
    modem.println("AT+CREG?");
    if (waitForResponse("+CREG: 0,1", 3000) || waitForResponse("+CREG: 0,5", 3000)) {
        return true; // đã đăng ký mạng
    }
    return false;
}

// --- Kiểm tra cường độ sóng ---
int GSM::checkSignalStrength()
{
    modem.println("AT+CSQ");
    uint32_t start = millis();
    String buffer;
    while (millis() - start < 3000) {
        while (modem.available()) {
            char c = modem.read();
            buffer += c;
            if (buffer.indexOf("+CSQ:") != -1) {
                int rssi = buffer.substring(buffer.indexOf(":") + 1, buffer.indexOf(",")).toInt();
                return rssi; // giá trị 0–31 (99 = không xác định)
            }
        }
    }
    return -1; // lỗi
}

// --- Lấy IMEI ---
String GSM::getIMEI()
{
    modem.println("AT+GSN");
    uint32_t start = millis();
    String buffer;
    while (millis() - start < 3000) {
        while (modem.available()) {
            char c = modem.read();
            buffer += c;
        }
    }
    buffer.trim();
    return buffer;
}