
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

// Blynk credentials (must be defined before including Blynk headers)
#define BLYNK_TEMPLATE_ID "TMPL6pVWQVaFd"
#define BLYNK_TEMPLATE_NAME "PZEM004T"
#define BLYNK_AUTH_TOKEN "mPEMmz_OoZqxVsnMwd9GEtzO1sDZGI5g"

#include <BlynkSimpleEsp32.h>
#include "pzem004t.h"
#include "GSM.h"
#include "OLED.h"

// --- TODO: set your WiFi credentials ---
#define WIFI_SSID "ChinaNet-GefHkJ"
#define WIFI_PASS "yxzd5926"

// --- TODO: set phone number for SMS (international format) ---
#define PHONE_NUMBER "+84327161236"

// UART pins (change to match your board wiring)
#define PZEM_RX_PIN 44
#define PZEM_TX_PIN 43
#define GSM_RX_PIN 18
#define GSM_TX_PIN 17

// Intervals
#define PZEM_READ_INTERVAL_MS 2000
#define BLYNK_SEND_INTERVAL_MS 5000
// SMS alert settings
// Previously SMS was sent periodically; now send only when power exceeds threshold
#define POWER_ALERT_THRESHOLD 300.0f      // W, change as needed
#define ALERT_COOLDOWN_MS     600000UL   // 10 minutes cooldown between alerts

// Virtual pins used for Blynk (mapped as you specified)
// V0 = LEDCONNECT (indicator if connected to Blynk)
#define VP_LED     V0
#define VP_VOLTAGE V1
#define VP_CURRENT V2
#define VP_POWER   V3
#define VP_ENERGY  V4
#define VP_FREQ    V5
#define VP_PF      V6

// Hardware serials
// Serial is USB debug
// Use Serial1 for PZEM, Serial2 for GSM

PZEM004T pzem(Serial1);
GSM gsm(Serial2, 115200);
OLED oled(128, 64);

struct SensorData {
	float voltage;
	float current;
	float power;
	float energy;
	float freq;
	float pf;
};

static SensorData g_data = {0};
static SemaphoreHandle_t g_data_mutex = NULL;
// energy is read from PZEM as Wh (watt-hours). Convert to kWh for display.
static float energy_offset_kwh = 0.0f; // baseline (kWh) to allow resetting displayed energy
static Preferences prefs;

// --- WHITELIST số điện thoại ---
const char* allowedNumbers[] = {"+84327161236", "+84977435825"};
const int allowedCount = sizeof(allowedNumbers) / sizeof(allowedNumbers[0]);

bool isAllowedSender(const String& sender) {
    for (int i = 0; i < allowedCount; i++) {
        if (sender.equals(allowedNumbers[i])) return true;
    }
    return false;
}

// WiFi diagnostic: print status, IP and DNS lookup for blynk.cloud
void wifiDiagnostics()
{
	Serial.print("WiFi status: ");
	Serial.println(WiFi.status());
	Serial.print("Local IP: ");
	Serial.println(WiFi.localIP());
	Serial.print("Gateway: ");
	Serial.println(WiFi.gatewayIP());
	Serial.print("Subnet: ");
	Serial.println(WiFi.subnetMask());

	Serial.print("Resolving blynk.cloud... ");
	IPAddress hostIP;
	if (WiFi.hostByName("blynk.cloud", hostIP)) {
		Serial.print("OK -> "); Serial.println(hostIP);
	} else {
		Serial.println("FAILED");
	}
}

// Tasks
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
			}
		}
		vTaskDelay(pdMS_TO_TICKS(PZEM_READ_INTERVAL_MS));
	}
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

// Note: Energy is sent on V4 (VP_ENERGY). Use V9 switch to reset/persist energy offset.

// Also provide V9 switch to reset energy (same behavior)
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

    		if (isAllowedSender(sender) && content.equalsIgnoreCase("REQUEST")) {
        		snprintf(msgbuf, sizeof(msgbuf),
                 	"Report: V=%.1fV I=%.3fA P=%.1fW E=%.3fkWh f=%.1fHz pf=%.2f",
                 	snapshot.voltage, snapshot.current, snapshot.power,
                 	snapshot.energy / 1000.0f, snapshot.freq, snapshot.pf);

        		Serial.printf("[GSM] Sending report to %s\n", sender.c_str());
        		sendWithRetries(sender.c_str(), msgbuf, 3);
    		} else {
        		Serial.println("[GSM] Unauthorized sender or invalid command");
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
}

void TaskDisplay(void *pvParameters)
{
	(void)pvParameters;
	char buf[64];

	for (;;) {
		SensorData snapshot;
		if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
			snapshot = g_data;
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

		snprintf(buf, sizeof(buf), "f: %.1f Hz  pf: %.2f", snapshot.freq, snapshot.pf);
		oled.printText(0, y, String(buf)); y += 10;

		oled.display();

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void setup()
{
	Serial.begin(115200);
	delay(100);

	// Configure UARTs with chosen pins
	Serial1.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
	Serial2.begin(115200, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);

	// Initialize GSM (this will re-init Serial2 if needed)
	gsm.begin(GSM_RX_PIN, GSM_TX_PIN);
	Serial2.println("AT+CMGF=1");
	delay(500);

	Serial2.println("AT+CNMI=2,2,0,0,0");
	delay(500);

	// Note: pzem.begin() would call Serial1.begin(baud) without pins,
	// so we already configured Serial1 with pins above and skip pzem.begin()

	// Mutex for shared sensor data
	g_data_mutex = xSemaphoreCreateMutex();

	// Connect to WiFi and Blynk
	WiFi.begin(WIFI_SSID, WIFI_PASS);
	unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
		Serial.print('.');
		delay(500);
	}
	Serial.println();
	if (WiFi.status() == WL_CONNECTED) {
		Serial.println("WiFi connected");
		Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
	} else {
		Serial.println("WiFi not connected; Blynk will try to reconnect in task");
		// It's still OK: TaskBlynk calls Blynk.run() which will reconnect when possible
		Blynk.config(BLYNK_AUTH_TOKEN);
	}

	// Print WiFi diagnostics to Serial
	wifiDiagnostics();

	// Preferences: load saved energy offset (kWh)
	prefs.begin("pzem", false);
	energy_offset_kwh = prefs.getFloat("energy_off", 0.0f);
	Serial.printf("Loaded energy_offset_kwh=%.3f\n", energy_offset_kwh);

	// Initialize OLED (SDA=13, SCL=12 typical for many ESP32 boards)
	if (!oled.begin(13, 12, 0x3C)) {
		Serial.println("OLED init failed");
	} else {
		Serial.println("OLED initialized");
	}

	// Create tasks
	xTaskCreatePinnedToCore(TaskPZEM, "PZEMTask", 4096, NULL, 2, NULL, 1);
	xTaskCreatePinnedToCore(TaskBlynk, "BlynkTask", 8192, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(TaskGSM,  "GSMTask",  4096, NULL, 1, NULL, 1);
	xTaskCreatePinnedToCore(TaskDisplay, "DisplayTask", 4096, NULL, 1, NULL, 1);

	// setup done; loop() will be empty
}

void loop()
{
	// All work is done in tasks
	vTaskDelay(pdMS_TO_TICKS(1000));
}
