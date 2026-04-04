#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "blynk_config.h"
#include <BlynkSimpleEsp32.h>
#include "globals.h"

// Task headers
#include "pzem_task.h"
#include "blynk_task.h"
#include "gsm_task.h"
#include "display_task.h"

#define WIFI_SSID "Phòng 603"
#define WIFI_PASS "88888888"

// Khởi tạo đối tượng toàn cục
PZEM004T pzem(Serial1);
GSM gsm(Serial2, 115200);
OLED oled(128, 64);

SensorData g_data = {0};
SemaphoreHandle_t g_data_mutex = NULL;
float energy_offset_kwh = 0.0f;
Preferences prefs;
// Default threshold; will be overwritten from prefs in setup()
float g_power_alert_threshold = 300.0f;

void wifiDiagnostics() {
    Serial.print("WiFi status: "); Serial.println(WiFi.status());
    Serial.print("Local IP: "); Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(9600);
    delay(100);
    
    Serial1.begin(9600, SERIAL_8N1, 14, 12);
    Serial2.begin(115200, SERIAL_8N1, 16, 17);

    gsm.begin(16, 17);
    g_data_mutex = xSemaphoreCreateMutex();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print('.');
    }

    if (WiFi.status() == WL_CONNECTED) {
        Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
    } else {
        Blynk.config(BLYNK_AUTH_TOKEN);
    }

    setupBlynkHandlers();
    
    prefs.begin("pzem", false);
    energy_offset_kwh = prefs.getFloat("energy_off", 0.0f);
    // Load saved power threshold (default 300W)
    g_power_alert_threshold = prefs.getFloat("power_th", 300.0f);

    if (!oled.begin(21, 22, 0x3C)) {
        Serial.println("OLED failed");
    }

    xTaskCreatePinnedToCore(TaskPZEM, "PZEMTask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(TaskBlynk, "BlynkTask", 8192, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskGSM, "GSMTask", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskDisplay, "DisplayTask", 4096, NULL, 1, NULL, 1);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}