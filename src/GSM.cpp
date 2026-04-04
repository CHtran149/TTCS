#include "GSM.h"

// --- Constructor ---
GSM::GSM(HardwareSerial &serial, uint32_t baudrate)
    : modem(serial), baud(baudrate) {}

// --- Khởi động modem ---
void GSM::begin(int rxPin, int txPin)
{
    modem.begin(baud, SERIAL_8N1, rxPin, txPin);
    delay(15000); // chờ modem ổn định

    // Basic initialization: disable echo, set SMS text mode,
    // and enable new-message indications so incoming SMS are
    // forwarded as +CMT: lines to the serial port.
    modem.println("AT");
    delay(500);
    modem.println("ATE0"); // disable echo
    delay(200);
    modem.println("AT+CMGF=1"); // SMS text mode
    delay(200);
    modem.println("AT+CNMI=2,2,0,0,0"); // deliver incoming SMS as +CMT
    delay(200);
    Serial.println("[GSM] Ready");
}

// --- Chờ ký tự phản hồi từ modem ---
void GSM::waitForChar(char target, uint32_t timeout)
{
    uint32_t start = millis();
    while (millis() - start < timeout)
    {
        if (modem.available())
        {
            char c = modem.read();
            Serial.write(c); // log debug
            if (c == target) return;
        }
    }
}

// Wait for a substring in modem response; returns true if found before timeout
// Non-blocking response scanner: accumulates modem bytes into an internal buffer
// and returns true if `needle` appears. Also extracts unsolicited +CMT headers
// into a pending slot so upper layer can read incoming SMS without losing data.
static String respBuf;
static String pendingSender;
static bool pendingHeader = false;

static void extractPendingHeaders() {
    int pos = 0;
    while ((pos = respBuf.indexOf("+CMT:")) >= 0) {
        int lineEnd = respBuf.indexOf('\n', pos);
        if (lineEnd < 0) break; // wait for full header line
        String header = respBuf.substring(pos, lineEnd);
        // parse sender
        int q1 = header.indexOf('"');
        int q2 = header.indexOf('"', q1 + 1);
        if (q1 >= 0 && q2 > q1) {
            pendingSender = header.substring(q1 + 1, q2);
            pendingHeader = true;
        }
        // remove header line from buffer
        respBuf = respBuf.substring(0, pos) + respBuf.substring(lineEnd + 1);
    }
}

static bool waitForResponse(HardwareSerial &modemRef, const char *needle, uint32_t /*timeout*/)
{
    while (modemRef.available()) {
        char c = modemRef.read();
        respBuf += c;
        Serial.write(c);
        if (respBuf.length() > 4096) respBuf = respBuf.substring(respBuf.length() - 2048);
    }
    // extract unsolicited +CMT headers into pending slot
    extractPendingHeaders();
    return respBuf.indexOf(needle) >= 0;
}

// --- Gửi SMS ---
bool GSM::sendSMS(const char *phone, const char *message)
{
    // Set text mode
    modem.println("AT+CMGF=1");
    if (!waitForResponse(modem, "OK", 2000)) {
        Serial.println("[GSM] no OK after CMGF");
        // continue attempt, but report failure
    }

    // Start send
    modem.print("AT+CMGS=\"");
    modem.print(phone);
    modem.println("\"");

    // wait for '>' prompt
    if (!waitForResponse(modem, ">", 5000)) {
        Serial.println("[GSM] no '>' prompt for CMGS");
        return false;
    }

    modem.print(message);
    delay(200);
    modem.write(26); // Ctrl+Z

    // After sending, modem replies with +CMGS: <mr> and then OK, or ERROR
    // After sending, modem replies with +CMGS: <mr> and then OK, or ERROR
    // Use a simple blocking loop but rely on non-blocking scanner to accumulate data.
    uint32_t start = millis();
    while (millis() - start < 15000) {
        if (waitForResponse(modem, "+CMGS", 0) || waitForResponse(modem, "OK", 0)) {
            return true;
        }
        if (waitForResponse(modem, "ERROR", 0)) {
            Serial.println("[GSM] CMGS ERROR");
            return false;
        }
        delay(10);
    }
    return false;
}


bool GSM::readSMS(String &sender, String &content)
{
    // If a pending header was previously detected by the scanner, and content now exists
    if (pendingHeader) {
        if (!modem.available()) return false; // wait for content next loop
        content = modem.readStringUntil('\n');
        content.trim();
        sender = pendingSender;
        pendingHeader = false;
        pendingSender = "";
        Serial.print("[SMS] From: ");
        Serial.println(sender);
        Serial.print("[SMS] Content: ");
        Serial.println(content);
        return true;
    }

    // Otherwise try to consume any immediate lines and detect a header+content on the fly
    if (!modem.available()) return false;
    String line = modem.readStringUntil('\n');
    line.trim();
    Serial.print("[RAW] "); Serial.println(line);
    if (line.startsWith("+CMT:")) {
        // parse sender
        int q1 = line.indexOf('"');
        int q2 = line.indexOf('"', q1 + 1);
        if (q1 >= 0 && q2 > q1) sender = line.substring(q1 + 1, q2);
        else sender = "UNKNOWN";
        // if content available immediately, read it
        if (!modem.available()) return false;
        content = modem.readStringUntil('\n');
        content.trim();
        Serial.print("[SMS] From: "); Serial.println(sender);
        Serial.print("[SMS] Content: "); Serial.println(content);
        return true;
    }
    return false;
}

bool GSM::scanFor(const char *needle) {
    return waitForResponse(modem, needle, 0);
}

void GSM::flushRX()
{
    respBuf = "";
    pendingSender = "";
    pendingHeader = false;
    while (modem.available()) {
        modem.read();
    }
}