#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

#include <atomic>
#include <cstdio>

#include "Adafruit_BMP085.h"

namespace {
    constexpr const char *kApSsid = "Space-Exe-Rocket";
    constexpr const char *kApPassword = "space2200ft";
    constexpr const char *kSensorDataPath = "/data/sensors.csv";
    constexpr unsigned long kSensorIntervalMs = 100;
    constexpr unsigned long kFlushIntervalMs = 1000;
    constexpr float kMockPressureHpa = 1001.76f;

    struct SensorReading {
        float pressure;
        float temperature1;
        float temperature2;
    };

    std::atomic<bool> recording{false};
    std::atomic<bool> clearDataPending{false};
    bool bmpReady = false;
    unsigned long lastFlush = 0;

    Adafruit_BMP085 bmp;
    AsyncWebServer server(80);
    AsyncEventSource events("/events");
    File sensorData;

    const char *recordingStateText() {
        return recording.load() ? "true" : "false";
    }

    void sendRecordingEvent() {
        events.send(recordingStateText(), "recording", millis());
    }

    void sendSensorEvent(const SensorReading &reading) {
        char json[128];
        snprintf(json, sizeof(json),
                 R"({"pressure":%.2f,"temperature1":%.2f,"temperature2":%.2f})",
                 reading.pressure, reading.temperature1, reading.temperature2);
        events.send(json, "sensor", millis());
    }

    SensorReading readSensors() {
        return {
            bmpReady ? bmp.readPressure() / 100.0f : kMockPressureHpa,
            static_cast<float>(analogRead(A0)) / 100.0f,
            static_cast<float>(analogRead(A1)) / 100.0f,
        };
    }

    void closeSensorLog() {
        if (!sensorData) {
            return;
        }
        sensorData.flush();
        sensorData.close();
        sensorData = File();
    }

    void ensureSensorDataFileExists() {
        if (LittleFS.exists(kSensorDataPath)) {
            return;
        }
        File file = LittleFS.open(kSensorDataPath, FILE_READ, true);
        if (file) {
            file.close();
        }
    }

    void logSensorSample(const unsigned long timestamp, const SensorReading &reading) {
        sensorData.printf("%lu,%f,%f,%f\n",
                          timestamp, reading.pressure, reading.temperature1, reading.temperature2);

        if (timestamp - lastFlush >= kFlushIntervalMs) {
            sensorData.flush();
            lastFlush = timestamp;
        }
    }

    void setupServer() {
        server.serveStatic("/", LittleFS, "/website/").setDefaultFile("index.html");

        server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/website/favicon.ico", "image/x-icon");
        });

        server.on("/toggle-recording", HTTP_POST, [](AsyncWebServerRequest *request) {
            recording.store(!recording.load());
            sendRecordingEvent();
            request->send(200);
        });

        server.on("/clear-data", HTTP_POST, [](AsyncWebServerRequest *request) {
            if (recording.load()) {
                request->send(409, "text/plain", "Cannot clear while recording");
                return;
            }

            clearDataPending.store(true);
            request->send(200);
        });
    }
} // namespace

void setup() {
    Serial.begin(115200);
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
    }
    if (!LittleFS.exists("/data")) {
        LittleFS.mkdir("/data");
    }
    ensureSensorDataFileExists();
    WiFi.softAP(kApSsid, kApPassword);
    pinMode(A0, INPUT);
    pinMode(A1, INPUT);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
    }

    setupServer();

    events.onConnect([](AsyncEventSourceClient *client) {
        if (client->lastId()) {
            Serial.printf("Client reconnecting, last event id: %u\n", client->lastId());
        }

        client->send(recordingStateText(), "recording", millis(), 10000);
    });
    bmpReady = bmp.begin();
    if (!bmpReady) {
        Serial.println("BMP085 init failed");
    }
    server.addHandler(&events);
    server.begin();
}

void loop() {
    static unsigned long lastSampleMs = 0;

    const unsigned long now = millis();

    if (clearDataPending.exchange(false)) {
        closeSensorLog();
        if (LittleFS.exists(kSensorDataPath)) {
            LittleFS.remove(kSensorDataPath);
        }
        ensureSensorDataFileExists();
    } else if (!recording.load() && sensorData) {
        closeSensorLog();
    }

    if (now - lastSampleMs < kSensorIntervalMs) {
        return;
    }
    lastSampleMs = now;

    const SensorReading reading = readSensors();
    sendSensorEvent(reading);

    if (recording.load()) {
        if (!sensorData) {
            sensorData = LittleFS.open(kSensorDataPath, FILE_APPEND);
            if (!sensorData) {
                Serial.println("Failed to open file for writing");
                recording.store(false);
                sendRecordingEvent();
            } else {
                lastFlush = now;
                logSensorSample(now, reading);
            }
        } else {
            logSensorSample(now, reading);
        }
    }
}
