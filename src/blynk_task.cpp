#include <Arduino.h>
// Prevent this translation unit from defining the global `Blynk` instance.
#define NO_GLOBAL_BLYNK
#include "blynk_config.h"
#include <BlynkSimpleEsp32.h>
#include "globals.h"
#include "blynk_task.h"

// Các chân Virtual Pin
#define VP_LED V0
#define VP_VOLTAGE V1
#define VP_CURRENT V2
#define VP_POWER V3
#define VP_ENERGY V4
#define VP_FREQ V5
#define VP_PF V6

#define BLYNK_SEND_INTERVAL_MS 5000

// Handler Reset năng lượng (V9)
BLYNK_WRITE(V9) {
    int val = param.asInt();
    if (val) {
        if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
            energy_offset_kwh = g_data.energy / 1000.0f;
            xSemaphoreGive(g_data_mutex);
            prefs.putFloat("energy_off", energy_offset_kwh);
            Blynk.virtualWrite(VP_ENERGY, 0);
            Serial.println("Energy Reset Done");
        }
    }
}

void setupBlynkHandlers() {
    Serial.println("Blynk handlers ready");
}

void TaskBlynk(void *pvParameters) {
    unsigned long lastSend = 0;
    for (;;) {
        Blynk.run();

        unsigned long now = millis();
        if (now - lastSend >= BLYNK_SEND_INTERVAL_MS) {
            SensorData snapshot;
            if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
                snapshot = g_data;
                xSemaphoreGive(g_data_mutex);
            }

            int conn = Blynk.connected() ? 1 : 0;
            Blynk.virtualWrite(VP_LED, conn);
            
            float energy_display = (snapshot.energy / 1000.0f) - energy_offset_kwh;
            if (energy_display < 0) energy_display = 0;

            Blynk.virtualWrite(VP_VOLTAGE, snapshot.voltage);
            Blynk.virtualWrite(VP_CURRENT, snapshot.current);
            Blynk.virtualWrite(VP_POWER, snapshot.power);
            Blynk.virtualWrite(VP_ENERGY, energy_display);
            Blynk.virtualWrite(VP_FREQ, snapshot.freq);
            Blynk.virtualWrite(VP_PF, snapshot.pf);

            lastSend = now;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}