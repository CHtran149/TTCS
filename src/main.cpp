#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include "blynk_config.h"
#include <BlynkSimpleEsp32.h>
#include "globals.h"

// Task headers
#include "pzem_task.h"
#include "blynk_task.h"
#include "gsm_task.h"
#include "display_task.h"
#include "cloud_task.h"

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
QueueHandle_t cloud_queue = NULL;

void wifiDiagnostics() {
    Serial.print("WiFi status: "); Serial.println(WiFi.status());
    Serial.print("Local IP: "); Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(9600);
    delay(100);
    
    Serial1.begin(9600, SERIAL_8N1, 14, 12); // PZEM RX=14, TX=12
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

    // Initialize time via NTP so timestamps are correct for cloud uploads (UTC+7)
    if (WiFi.status() == WL_CONNECTED) {
        // Set timezone to Vietnam (UTC+7) and initialize NTP
        setenv("TZ", "Asia/Ho_Chi_Minh", 1);
        tzset();
        configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
        Serial.print("Waiting for NTP time");
        time_t now = time(NULL);
        unsigned long start = millis();
        while (now < 1600000000 && millis() - start < 10000) {
            delay(500);
            Serial.print('.');
            now = time(NULL);
        }
        Serial.println();
        if (now < 1600000000) {
            Serial.println("Warning: NTP time not set");
        } else {
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            Serial.printf("Current time: %s", asctime(&timeinfo));
        }
    }

    setupBlynkHandlers();
    
    prefs.begin("pzem", false);
    energy_offset_kwh = prefs.getFloat("energy_off", 0.0f);
    // Load saved power threshold (default 300W)
    g_power_alert_threshold = prefs.getFloat("power_th", 300.0f);

    if (!oled.begin(21, 22, 0x3C)) {
        Serial.println("OLED failed");
    }

    // Create cloud queue and start cloud task
    cloud_queue = xQueueCreate(10, sizeof(CloudData));
    // if (cloud_queue != NULL) {
    //     xTaskCreatePinnedToCore(TaskCloud, "CloudTask", 8192, NULL, 1, NULL, 1);
    // } else {
    //     Serial.println("Failed to create cloud_queue");
    // }

    xTaskCreatePinnedToCore(TaskPZEM, "PZEMTask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(TaskBlynk, "BlynkTask", 8192, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskGSM, "GSMTask", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskDisplay, "DisplayTask", 4096, NULL, 1, NULL, 1);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}