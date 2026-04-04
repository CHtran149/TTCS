#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Preferences.h>
#include "blynk_config.h"
#include "pzem004t.h" // Nhớ include các class để trình biên dịch hiểu
#include "GSM.h"
#include "OLED.h"

struct SensorData {
    float voltage;
    float current;
    float power;
    float energy;
    float freq;
    float pf;
};

// Khai báo extern để dùng chung giữa các file .cpp
extern SensorData g_data;
extern SemaphoreHandle_t g_data_mutex;
extern float energy_offset_kwh;
extern Preferences prefs;

extern PZEM004T pzem;
extern GSM gsm;
extern OLED oled;

#endif