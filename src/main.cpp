#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

#include <atomic>
#include <cstdio>

namespace {
    constexpr const char* kApSsid = "Space-Exe-Rocket";
    constexpr const char* kApPassword = "space2200ft";
    constexpr unsigned long kSensorIntervalMs = 100;
    constexpr float kMockPressureHpa = 1001.76f;

    std::atomic<bool> recording{false};

    AsyncWebServer server(80);
    AsyncEventSource events("/events");

    const char* recordingStateText() {
        return recording.load() ? "true" : "false";
    }

    void sendRecordingEvent() {
        events.send(recordingStateText(), "recording", millis());
    }

    void sendSensorEvent(const float pressure, const float temperature1, const float temperature2) {
        char json[128];
        snprintf(json, sizeof(json),
                 R"({"pressure":%.2f,"temperature1":%.2f,"temperature2":%.2f})",
                 pressure, temperature1, temperature2);
        events.send(json, "sensor", millis());
    }

    void setupServer() {
        server.serveStatic("/", LittleFS, "/website/").setDefaultFile("index.html");

        server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(LittleFS, "/website/favicon.ico", "image/x-icon");
        });

        server.on("/toggle-recording", HTTP_POST, [](AsyncWebServerRequest* request) {
            recording.store(!recording.load());
            sendRecordingEvent();
            request->send(200);
        });
    }
} // namespace

void setup() {
    Serial.begin(115200);
    WiFi.softAP(kApSsid, kApPassword);
    pinMode(A0, INPUT);
    pinMode(A1, INPUT);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
    }

    setupServer();

    events.onConnect([](AsyncEventSourceClient* client) {
        if (client->lastId()) {
            Serial.printf("Client reconnecting, last event id: %u\n", client->lastId());
        }

        client->send(recordingStateText(), "recording", millis(), 10000);
    });

    server.addHandler(&events);
    server.begin();
}

void loop() {
    static unsigned long lastSend = 0;
    const unsigned long now = millis();

    if (now - lastSend < kSensorIntervalMs) {
        return;
    }
    lastSend = now;

    sendSensorEvent(kMockPressureHpa,
                    analogRead(A0) / 100.0f,
                    analogRead(A1) / 100.0f);
}
