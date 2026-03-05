#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class OLED {
public:
    struct Config {
        uint8_t width;
        uint8_t height;
        uint8_t address;
        uint8_t sda;
        uint8_t scl;
        bool valid;
    };

private:
    Adafruit_SSD1306* _display;
    Config _config;

public:
    OLED(uint8_t width = 128, uint8_t height = 64);

    bool begin(uint8_t sda, uint8_t scl, uint8_t address = 0x3C);

    void clear();
    void printText(int x, int y, const String &text, uint8_t size = 1);
    void display();

    Config getConfig();
};

#endif