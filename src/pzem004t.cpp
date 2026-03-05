#include "pzem004t.h"

//====================================================
// Frame Modbus PZEM004T đọc tất cả các thông số
//====================================================
static const uint8_t PZEM_CMD[8] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x70, 0x0D};

PZEM004T::PZEM004T(HardwareSerial& serial, uint8_t addr)
    : _serial(&serial), _addr(addr),
      _voltage(0), _current(0), _power(0),
      _energy(0), _freq(0), _pf(0)
{
}

void PZEM004T::begin(uint32_t baudrate)
{
    _serial->begin(baudrate);
}

//====================================================
// Đọc dữ liệu từ PZEM
//====================================================
bool PZEM004T::read()
{
    uint8_t resp[25] = {0};

    // Gửi lệnh đọc
    _serial->write(PZEM_CMD, 8);

    // Đọc 25 byte phản hồi
    unsigned long start = millis();
    int index = 0;
    while (index < 25 && (millis() - start < 100)) { // timeout 100ms
        if (_serial->available()) {
            resp[index++] = _serial->read();
        }
    }
    if (index != 25) return false;

    // CRC16 kiểm tra
    uint16_t crc_calc = modbusCRC(resp, 23);
    uint16_t crc_recv = resp[23] | (resp[24] << 8);
    if (crc_calc != crc_recv) return false;

    // Giải mã dữ liệu (theo datasheet PZEM v3)
    _voltage = ((resp[3] << 8) | resp[4]) / 10.0f;

    uint32_t icur = ((uint32_t)resp[7] << 24) | ((uint32_t)resp[8] << 16) |
                    ((uint32_t)resp[5] << 8)  | resp[6];
    _current = icur / 1000.0f;

    uint32_t ipow = ((uint32_t)resp[11] << 24) | ((uint32_t)resp[12] << 16) |
                    ((uint32_t)resp[9] << 8) | resp[10];
    _power = ipow / 10.0f;

    uint32_t ien = ((uint32_t)resp[16] << 24) | ((uint32_t)resp[15] << 16) |
                   ((uint32_t)resp[14] << 8) | resp[13];
    _energy = (float)ien;

    _freq = ((resp[17] << 8) | resp[18]) / 10.0f;
    _pf   = ((resp[19] << 8) | resp[20]) / 100.0f;

    return true;
}

//====================================================
// Getter
//====================================================
float PZEM004T::voltage()   const { return _voltage; }
float PZEM004T::current()   const { return _current; }
float PZEM004T::power()     const { return _power; }
float PZEM004T::energy()    const { return _energy; }
float PZEM004T::frequency() const { return _freq; }
float PZEM004T::pf()        const { return _pf; }

//====================================================
// CRC16 Modbus
//====================================================
uint16_t PZEM004T::modbusCRC(uint8_t *buf, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}
