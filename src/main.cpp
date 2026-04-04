
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

// Task headers
#include "pzem_task.h"
#include "blynk_task.h"
#include "gsm_task.h"
#include "display_task.h"

// --- TODO: set your WiFi credentials ---
#define WIFI_SSID "Phòng 603"
#define WIFI_PASS "88888888"

// --- TODO: set phone number for SMS (international format) ---
#define PHONE_NUMBER "+84327161236"

// UART pins (change to match your board wiring)
#define PZEM_RX_PIN 14
#define PZEM_TX_PIN 12
#define GSM_RX_PIN 16
#define GSM_TX_PIN 17
#define OLED_SCK_PIN 22
#define OLED_SDA_PIN 21
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
// const char* allowedNumbers[] = {"+84327161236", "+84977435825"};
// const int allowedCount = sizeof(allowedNumbers) / sizeof(allowedNumbers[0]);

// bool isAllowedSender(const String& sender) {
//     for (int i = 0; i < allowedCount; i++) {
//         if (sender.equals(allowedNumbers[i])) return true;
//     }
//     return false;
// }

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

// Tasks are implemented in separate files:
// - TaskPZEM in pzem_task.cpp
// - TaskBlynk in blynk_task.cpp
// - TaskGSM in gsm_task.cpp
// - TaskDisplay in display_task.cpp







void setup()
{
	Serial.begin(9600);
	delay(100);
	Serial.println("Starting up...");
	// Configure UARTs with chosen pins
	Serial1.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
	Serial2.begin(115200, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);

	// Initialize GSM (this will re-init Serial2 if needed)
	gsm.begin(GSM_RX_PIN, GSM_TX_PIN);

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

	// Setup Blynk event handlers
	setupBlynkHandlers();

	// Print WiFi diagnostics to Serial
	wifiDiagnostics();

	// Preferences: load saved energy offset (kWh)
	prefs.begin("pzem", false);
	energy_offset_kwh = prefs.getFloat("energy_off", 0.0f);
	Serial.printf("Loaded energy_offset_kwh=%.3f\n", energy_offset_kwh);

	// Initialize OLED (SDA=13, SCL=12 typical for many ESP32 boards)
	if (!oled.begin(OLED_SDA_PIN, OLED_SCK_PIN, 0x3C)) {
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
