#include <Arduino.h>
#include <BlynkSimpleEsp32.h>
#include "globals.h"
#include "blynk_task.h"

// Blynk virtual pin definitions (must match main.cpp defines)
#define VP_LED     V0
#define VP_VOLTAGE V1
#define VP_CURRENT V2
#define VP_POWER   V3
#define VP_ENERGY  V4
#define VP_FREQ    V5
#define VP_PF      V6

// Interval for sending Blynk data
#define BLYNK_SEND_INTERVAL_MS 5000

// Blynk virtual pin handler for energy reset (V9)
BLYNK_WRITE(V9)
{
	int val = param.asInt();
	if (val) {
		if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
			energy_offset_kwh = g_data.energy / 1000.0f;
			xSemaphoreGive(g_data_mutex);
			Serial.printf("Energy reset(V9): offset_kwh=%.3f\n", energy_offset_kwh);
			prefs.putFloat("energy_off", energy_offset_kwh);
			Blynk.virtualWrite(VP_ENERGY, 0);
		}
	}
}

void setupBlynkHandlers()
{
	// This function is called after Blynk is initialized
	// Blynk event handlers are already registered via BLYNK_WRITE macros
	Serial.println("Blynk handlers setup complete");
}

void TaskBlynk(void *pvParameters)
{
	(void)pvParameters;
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

			// Update connection LED (1 = connected, 0 = not connected)
			int conn = Blynk.connected() ? 1 : 0;
			Blynk.virtualWrite(VP_LED, conn);
			
			// Debug log: show whether we're connected and the values about to be sent
			float snapshot_energy_kwh = snapshot.energy / 1000.0f;
			float energy_display = snapshot_energy_kwh - energy_offset_kwh;
			if (energy_display < 0) energy_display = 0;
			Serial.printf("Blynk send (connected=%d) -> V=%.1f I=%.3f P=%.1f E=%.3fkWh f=%.1f pf=%.2f\n",
				conn, snapshot.voltage, snapshot.current, snapshot.power,
				energy_display, snapshot.freq, snapshot.pf);

			Blynk.virtualWrite(VP_VOLTAGE, snapshot.voltage);
			Blynk.virtualWrite(VP_CURRENT, snapshot.current);
			Blynk.virtualWrite(VP_POWER,   snapshot.power);
			// send energy adjusted by offset (convert Wh -> kWh first)
			Blynk.virtualWrite(VP_ENERGY,  energy_display);
			Blynk.virtualWrite(VP_FREQ,    snapshot.freq);
			Blynk.virtualWrite(VP_PF,      snapshot.pf);

			lastSend = now;
		}

		vTaskDelay(pdMS_TO_TICKS(100)); // keep Blynk.run() responsive
	}
}
