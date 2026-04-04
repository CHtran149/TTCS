#include <Arduino.h>
#include "globals.h"
#include "OLED.h"
#include "display_task.h"

void TaskDisplay(void *pvParameters)
{
	(void)pvParameters;
	char buf[64];

	for (;;) {
		SensorData snapshot;
		float current_threshold = 0.0f;
		if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
			snapshot = g_data;
			current_threshold = g_power_alert_threshold;
			xSemaphoreGive(g_data_mutex);
		}

		// Prepare display
		oled.clear();

		// Line spacing for 6 lines on 64px height
		int y = 0;
		snprintf(buf, sizeof(buf), "V: %.1f V", snapshot.voltage);
		oled.printText(0, y, String(buf)); y += 10;

		snprintf(buf, sizeof(buf), "I: %.3f A", snapshot.current);
		oled.printText(0, y, String(buf)); y += 10;

		snprintf(buf, sizeof(buf), "P: %.1f W", snapshot.power);
		oled.printText(0, y, String(buf)); y += 10;

		snprintf(buf, sizeof(buf), "E: %.3f kWh", snapshot.energy / 1000.0f);
		oled.printText(0, y, String(buf)); y += 10;

		snprintf(buf, sizeof(buf), "TH: %.1fW", current_threshold);
		oled.printText(0, y, String(buf)); y += 10;

		snprintf(buf, sizeof(buf), "f: %.1f Hz  pf: %.2f", snapshot.freq, snapshot.pf);
		oled.printText(0, y, String(buf)); y += 10;

		oled.display();

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
