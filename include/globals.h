#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Preferences.h>

// Sensor data structure
struct SensorData {
	float voltage;
	float current;
	float power;
	float energy;
	float freq;
	float pf;
};

// Extern declarations for global variables
extern SensorData g_data;
extern SemaphoreHandle_t g_data_mutex;
extern float energy_offset_kwh;
extern Preferences prefs;

// Extern object declarations
class PZEM004T;
class GSM;
class OLED;

extern PZEM004T pzem;
extern GSM gsm;
extern OLED oled;

#endif // GLOBALS_H
