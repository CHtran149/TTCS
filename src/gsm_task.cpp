#include <Arduino.h>
#include "globals.h"
#include "GSM.h"
#include "gsm_task.h"

// Configuration for GSM task
// ALERT_COOLDOWN_MS: 10 minutes cooldown between alerts
#define ALERT_COOLDOWN_MS     600000UL
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
		if (snapshot.power > g_power_alert_threshold && (now - lastAlert >= ALERT_COOLDOWN_MS)) {
			snprintf(msgbuf, sizeof(msgbuf),
				 "ALERT: Power exceeded %.1fW -> P=%.1fW V=%.1fV I=%.3fA",
				 g_power_alert_threshold, snapshot.power, snapshot.voltage, snapshot.current);

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

			content.trim();
			String contentUp = content;
			contentUp.toUpperCase();

			// REQUEST or REQ -> reply with data + current threshold
			if (contentUp.equals("REQUEST") || contentUp.equals("REQ")) {
				snprintf(msgbuf, sizeof(msgbuf),
					"Report: V=%.1fV I=%.3fA P=%.1fW E=%.3fkWh f=%.1fHz pf=%.2f TH=%.1fW",
					snapshot.voltage, snapshot.current, snapshot.power,
					snapshot.energy / 1000.0f, snapshot.freq, snapshot.pf, g_power_alert_threshold);

				Serial.printf("[GSM] Replying to %s\n", sender.c_str());
				if (!sendWithRetries(sender.c_str(), msgbuf, 3)) {
					Serial.println("[GSM] Warning: failed to send REQUEST reply");
				}
			}

			// SET command handling (e.g., "SET POWER=350")
			else if (contentUp.startsWith("SET")) {
				bool handled = false;
				// find POWER= in case-insensitive way
				int p = contentUp.indexOf("POWER=");
				if (p >= 0) {
					// extract numeric substring from original content to preserve digits
					int startIdx = p + 6; // position after 'POWER='
					String numStr = content.substring(startIdx);
					numStr.trim();
					float newTh = numStr.toFloat();
					if (newTh > 0.0f) {
						// persist immediately
						g_power_alert_threshold = newTh;
						prefs.putFloat("power_th", g_power_alert_threshold);
						handled = true;
						snprintf(msgbuf, sizeof(msgbuf), "OK: Power threshold set to %.1fW", g_power_alert_threshold);
						Serial.printf("[GSM] Power threshold updated to %.1fW by %s\n", g_power_alert_threshold, sender.c_str());
					} else {
						snprintf(msgbuf, sizeof(msgbuf), "ERR: invalid value '%s'", numStr.c_str());
						handled = true;
					}
				}

				if (handled) {
					if (!sendWithRetries(sender.c_str(), msgbuf, 3)) {
						Serial.println("[GSM] Warning: failed to send SET confirmation");
					}
				} else {
					Serial.println("[GSM] SET command not recognized or bad format");
					if (!sendWithRetries(sender.c_str(), "ERR: SET format invalid", 1)) {
						Serial.println("[GSM] Warning: failed to send SET error");
					}
				}
			}

			else {
				Serial.println("[GSM] SMS content not recognized, ignoring...");
			}

		}
	}
		
		// --- Báo cáo định kỳ 5 phút ---
		// if (now - lastReport >= 300000UL) { // 300000 ms = 5 phút
		// 	snprintf(msgbuf, sizeof(msgbuf),
		// 			 "Report: V=%.1fV I=%.3fA P=%.1fW E=%.3fkWh f=%.1fHz pf=%.2f",
		// 			 snapshot.voltage, snapshot.current, snapshot.power,
		// 			 snapshot.energy / 1000.0f, snapshot.freq, snapshot.pf);

		// 	Serial.printf("Sending SMS report to %s: %s\n", PHONE_NUMBER, msgbuf);

		// 	if (sendWithRetries(PHONE_NUMBER, msgbuf, 3)) {
		// 		lastReport = now;
		// 	} else {
		// 		Serial.println("SMS report failed");
		// 	}
		// }

		vTaskDelay(pdMS_TO_TICKS(10000)); // kiểm tra mỗi 10 giây
}

