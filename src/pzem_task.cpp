#include <Arduino.h>
#include "globals.h"
#include "pzem004t.h"
#include "pzem_task.h"
#include "cloud_task.h"
#include <time.h>

// Define PZEM_READ_INTERVAL_MS
#define PZEM_READ_INTERVAL_MS 2000

void TaskPZEM(void *pvParameters)
{
	(void)pvParameters;
	for (;;) {
		// Read from PZEM
		if (pzem.read()) {
			if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
				g_data.voltage = pzem.voltage();
				g_data.current = pzem.current();
				g_data.power   = pzem.power();
				g_data.energy  = pzem.energy();
				g_data.freq    = pzem.frequency();
				g_data.pf      = pzem.pf();
				xSemaphoreGive(g_data_mutex);

				// Debug log: print the freshly read sensor values (energy converted to kWh)
				float energy_kwh = g_data.energy / 1000.0f;
				Serial.printf("PZEM read -> V=%.1fV I=%.3fA P=%.1fW E=%.3fkWh f=%.1fHz pf=%.2f\n",
					g_data.voltage, g_data.current, g_data.power,
					energy_kwh, g_data.freq, g_data.pf);

				// Push to cloud queue (non-blocking)
				if (cloud_queue != NULL) {
					CloudData cd;
					cd.epoch = (uint32_t)time(NULL);
					cd.voltage = g_data.voltage;
					cd.current = g_data.current;
					cd.power = g_data.power;
					cd.energy = g_data.energy / 1000.0f; // convert to kWh if needed
					xQueueSend(cloud_queue, &cd, 0);
				}
			}
		}
		vTaskDelay(pdMS_TO_TICKS(PZEM_READ_INTERVAL_MS));
	}
}
