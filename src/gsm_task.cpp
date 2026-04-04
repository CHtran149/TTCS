#include <Arduino.h>
#include "globals.h"
#include "GSM.h"
#include "gsm_task.h"

// Configuration for GSM task
#define POWER_ALERT_THRESHOLD 300.0f      // W, change as needed
#define ALERT_COOLDOWN_MS     600000UL    // 10 minutes cooldown between alerts
#define PHONE_NUMBER "+84327161236"

void TaskGSM(void *pvParameters)
{
	(void)pvParameters;
	unsigned long lastAlert = 0;   // thời điểm gửi cảnh báo gần nhất
	unsigned long lastReport = 0;  // thời điểm gửi báo cáo gần nhất
	char msgbuf[256];

	auto sendWithRetries = [&](const char *phone, const char *message, int retries)->bool {
		for (int i = 0; i < retries; ++i) {
			Serial.printf("[GSM] Sending SMS attempt %d/%d...\n", i+1, retries);
			if (gsm.sendSMS(phone, message)) {
				Serial.println("[GSM] SMS sent");
				return true;
			}
			Serial.println("[GSM] SMS send failed, retrying...");
			vTaskDelay(pdMS_TO_TICKS(3000));
		}
		return false;
	};

	for (;;) {
		unsigned long now = millis();

		// Lấy snapshot dữ liệu cảm biến
		SensorData snapshot;
		if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
			snapshot = g_data;
			xSemaphoreGive(g_data_mutex);
		}

		// --- Cảnh báo vượt ngưỡng ---
		if (snapshot.power > POWER_ALERT_THRESHOLD && (now - lastAlert >= ALERT_COOLDOWN_MS)) {
			snprintf(msgbuf, sizeof(msgbuf),
					 "ALERT: Power exceeded %.1fW -> P=%.1fW V=%.1fV I=%.3fA",
					 POWER_ALERT_THRESHOLD, snapshot.power, snapshot.voltage, snapshot.current);

			Serial.printf("ALERT: sending SMS to %s: %s\n", PHONE_NUMBER, msgbuf);

			if (sendWithRetries(PHONE_NUMBER, msgbuf, 3)) {
				lastAlert = now;
			} else {
				Serial.println("SMS alert failed after retries");
			}
		}

		String sender, content;
		while (gsm.readSMS(sender, content)) {
    		Serial.printf("[GSM] SMS received from %s: %s\n", sender.c_str(), content.c_str());

    		if (content.equalsIgnoreCase("REQUEST")) {
        		snprintf(msgbuf, sizeof(msgbuf),
                 		"Report: V=%.1fV I=%.3fA P=%.1fW E=%.3fkWh f=%.1fHz pf=%.2f",
                 		snapshot.voltage, snapshot.current, snapshot.power,
                 		snapshot.energy / 1000.0f, snapshot.freq, snapshot.pf);

        		Serial.printf("[GSM] Sending report to %s\n", sender.c_str());
        		sendWithRetries(sender.c_str(), msgbuf, 3);
    		}
		}
		
		// --- Báo cáo định kỳ 5 phút ---
		if (now - lastReport >= 300000UL) { // 300000 ms = 5 phút
			snprintf(msgbuf, sizeof(msgbuf),
					 "Report: V=%.1fV I=%.3fA P=%.1fW E=%.3fkWh f=%.1fHz pf=%.2f",
					 snapshot.voltage, snapshot.current, snapshot.power,
					 snapshot.energy / 1000.0f, snapshot.freq, snapshot.pf);

			Serial.printf("Sending SMS report to %s: %s\n", PHONE_NUMBER, msgbuf);

			if (sendWithRetries(PHONE_NUMBER, msgbuf, 3)) {
				lastReport = now;
			} else {
				Serial.println("SMS report failed");
			}
		}

		vTaskDelay(pdMS_TO_TICKS(10000)); // kiểm tra mỗi 10 giây
	}
}
