#ifndef PZEM004T_H
#define PZEM004T_H

#include <Arduino.h>

class PZEM004T {
public:
    // Constructor: truyền vào HardwareSerial, địa chỉ PZEM (mặc định 0x01)
    PZEM004T(HardwareSerial& serial, uint8_t addr = 0x01);

    // Khởi tạo UART (baudrate 9600)
    void begin(uint32_t baudrate = 9600);

    // Đọc tất cả dữ liệu từ PZEM, trả về true nếu thành công
    bool read();

    // Getter
    float voltage()   const;
    float current()   const;
    float power()     const;
    float energy()    const;
    float frequency() const;
    float pf()        const;

private:
    HardwareSerial* _serial;
    uint8_t _addr;

    float _voltage;
    float _current;
    float _power;
    float _energy;
    float _freq;
    float _pf;

    // Modbus CRC16
    static uint16_t modbusCRC(uint8_t *buf, uint8_t len);
};

#endif
