#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "cloud_task.h"

// TODO: Replace with your deployed Apps Script URL
// Đảm bảo không có khoảng trắng thừa ở hai đầu chuỗi
static const char* GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbyHOw4c0bI1NebeidMBSa6j_0_fkjFigWu7f-qpwmdQ2Vx-MNZ7NPqRTANrRWTSkD3cDw/exec";
// Optional shared secret token for basic authentication. Replace before deploy.
static const char* CLOUD_SHARED_TOKEN = "REPLACE_WITH_SECRET_TOKEN";
// Number of retries/delays can be tuned
void TaskCloud(void *pvParameters) {
    (void)pvParameters;
    CloudData data;

    for (;;) {
        if (cloud_queue == NULL) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (xQueueReceive(cloud_queue, &data, portMAX_DELAY) == pdTRUE) {
            // 1. Xử lý thời gian
            time_t t = data.epoch ? (time_t)data.epoch : time(NULL);
            struct tm tm_info;
            localtime_r(&t, &tm_info);
            char timestr[32];
            strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm_info);

            // 2. Lambda function để URL Encode
            auto urlEncode = [](const String &s) {
                String enc;
                char hex[] = "0123456789ABCDEF";
                for (size_t i = 0; i < s.length(); ++i) {
                    char c = s[i];
                    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
                         c == '-' || c == '_' || c == '.' || c == '~') {
                        enc += c;
                    } else if (c == ' ') {
                        enc += '+';
                    } else {
                        enc += '%';
                        enc += hex[(c >> 4) & 0xF];
                        enc += hex[c & 0xF];
                    }
                }
                return enc;
            };

            // 3. Xây dựng Payload dạng Form
            bool deviceTimeValid = (t >= 1600000000);
            String payload = "";
            if (deviceTimeValid) {
                payload += "date=" + urlEncode(String(timestr));
            } else {
                payload += "use_server_time=1";
                Serial.println("Cloud: device time invalid, request server time");
            }

            payload += (payload.length() ? "&" : "");
            payload += "voltage=" + urlEncode(String(data.voltage, 2));
            payload += "&current=" + urlEncode(String(data.current, 3));
            payload += "&power=" + urlEncode(String(data.power, 2));
            payload += "&energy=" + urlEncode(String(data.energy, 3));

            // 4. Append token if configured and send with retries + backoff
            if (CLOUD_SHARED_TOKEN && strlen(CLOUD_SHARED_TOKEN) > 0) {
                payload += "&token=" + urlEncode(String(CLOUD_SHARED_TOKEN));
            }

            const int maxRetries = 4;
            bool sent = false;

            for (int attempt = 1; attempt <= maxRetries && !sent; attempt++) {
                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("Cloud: WiFi not connected, waiting...");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    continue;
                }

                Serial.printf("Cloud: attempt %d payload=%s\n", attempt, payload.c_str());

                WiFiClientSecure client;
                // For quick testing we keep insecure; for production use client.setCACert(root_ca_pem)
                client.setInsecure();
                HTTPClient https;

                if (https.begin(client, GOOGLE_SCRIPT_URL)) {
                    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
                    https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
                    https.setTimeout(20000);

                    int httpCode = https.POST(payload);
                    String resp = "";
                    if (httpCode > 0) resp = https.getString();

                    if (httpCode > 0) {
                        Serial.printf("Cloud Attempt %d - Code %d\n", attempt, httpCode);
                        Serial.println(resp);
                        if (httpCode >= 200 && httpCode < 300) sent = true;
                    } else {
                        Serial.printf("Cloud Attempt %d - Failed: %s\n", attempt, https.errorToString(httpCode).c_str());
                    }
                    https.end();
                } else {
                    Serial.println("Cloud: unable to begin HTTPS");
                }

                if (!sent) {
                    int backoffMs = 2000 * attempt; // 2s,4s,6s...
                    Serial.printf("Cloud: retrying after %d ms\n", backoffMs);
                    vTaskDelay(pdMS_TO_TICKS(backoffMs));
                }
            }

            if (!sent) {
                Serial.println("Cloud: giving up after retries");
            }
        }
    }
}