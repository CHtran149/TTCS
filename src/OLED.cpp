#include "OLED.h"

// --- Khởi tạo ---
OLED::OLED(uint8_t width, uint8_t height) {
    _config.width = width;
    _config.height = height;
    _config.address = 0x3C;
    _config.sda = 0;
    _config.scl = 0;
    _config.valid = false;

    _display = new Adafruit_SSD1306(width, height, &Wire, -1);
}

// --- Khởi động màn hình ---
bool OLED::begin(uint8_t sda, uint8_t scl, uint8_t address) { 

    _config.sda = sda;
    _config.scl = scl;
    _config.address = address;

    Wire.begin(sda, scl);

    if (!_display->begin(SSD1306_SWITCHCAPVCC, address)) {
        _config.valid = false;
        return false;
    }

    _display->clearDisplay();
    _display->setTextSize(1);
    _display->setTextColor(SSD1306_WHITE);
    _display->display();

    _config.valid = true;
    return true;
}

// --- Xóa màn hình ---
void OLED::clear() {
    if (!_config.valid) return;
    _display->clearDisplay();
}

// --- In chữ lên màn hình ---
void OLED::printText(int x, int y, const String &text, uint8_t size) {
    if (!_config.valid) return;

    _display->setTextSize(size);
    _display->setCursor(x, y);
    _display->print(text);
}

// --- Cập nhật hiển thị ---
void OLED::display() {
    if (!_config.valid) return;
    _display->display();
}

// --- Lấy cấu hình hiện tại ---
OLED::Config OLED::getConfig() {
    return _config;
}