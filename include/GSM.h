#ifndef GSM_H
#define GSM_H

#include <Arduino.h>

class GSM {
private:
    HardwareSerial &modem;
    uint32_t baud;

    // Hàm chờ phản hồi chuỗi đầy đủ
    bool waitForResponse(const char *target, uint32_t timeout = 5000);

public:
    GSM(HardwareSerial &serial, uint32_t baudrate);

    // Khởi động modem, kiểm tra phản hồi AT
    bool begin(int rxPin, int txPin);

    // Gửi SMS với retry
    bool sendSMS(const char *phone, const char *message, int retries = 3);

    // Kiểm tra modem đã đăng ký mạng chưa
    bool isRegistered();

    // Kiểm tra cường độ sóng
    int checkSignalStrength();

    // Lấy IMEI của modem
    String getIMEI();
};

#endif