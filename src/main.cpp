#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <atomic>
#include <ArduinoJson.h>
#include "Adafruit_BMP085.h"

namespace {
    constexpr const char *kApSsid = "Space-Exe-Rocket";
    constexpr const char *kApPassword = "space2200ft";
    constexpr const char *kSensorDataPath = "/data/sensors.csv";
    constexpr unsigned long kSensorIntervalMs = 100;
    constexpr unsigned long kFlushIntervalMs = 1000;
    constexpr float kMockPressureHpa = 1001.76f;
    constexpr int kSensorPrecision = 2;

    struct SensorReading {
        float pressure;
        float altitude;
        float temperature1;
        float temperature2;
    };

    std::atomic<bool> recording{false};
    std::atomic<bool> clearDataPending{false};
    std::atomic<bool> calibratePending{false};
    std::atomic<float> calibratedPressure{0.0f};
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
        JsonDocument doc;
        doc["pressure"] = reading.pressure;
        doc["altitude"] = reading.altitude;
        doc["temperature1"] = reading.temperature1;
        doc["temperature2"] = reading.temperature2;
        String json;
        serializeJson(doc, json);
        events.send(json, "sensor", millis());
    }

    void sendCalibrationEvent() {
        JsonDocument doc;
        doc["calibratedPressure"] = calibratedPressure.load() / 100.0f;
        String json;
        serializeJson(doc, json);
        events.send(json, "calibration", millis());
    }

    float pressureToAltitude(const float pressurePa, const float sealevelPressurePa) {
        if (sealevelPressurePa <= 0.0f) {
            return 0.0f;
        }
        return 44330 * (1.0f - pow(pressurePa / sealevelPressurePa, 0.1903));
    }

    SensorReading readSensors() {
        if (!bmpReady) {
            return {
                .pressure = kMockPressureHpa,
                .altitude = 0.0f,
                .temperature1 = 0.0f,
                .temperature2 = static_cast<float>(analogRead(A1)) / 100.0f,
            };
        }

        const float pressurePa = bmp.readPressure();
        const float sealevelPressurePa = calibratedPressure.load();

        return {
            .pressure = pressurePa / 100.0f,
            .altitude = pressureToAltitude(pressurePa, sealevelPressurePa),
            .temperature1 = bmp.readTemperature(),
            .temperature2 = static_cast<float>(analogRead(A1)) / 100.0f,
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
        File file = LittleFS.open(kSensorDataPath, FILE_WRITE, true);
        file.printf("timestamp,pressure,altitude,temperature1,temperature2\n");
        if (file) {
            file.close();
        }
    }

    void runPendingClearData() {
        if (!clearDataPending.exchange(false)) {
            return;
        }

        closeSensorLog();
        if (LittleFS.exists(kSensorDataPath)) {
            LittleFS.remove(kSensorDataPath);
        }
        ensureSensorDataFileExists();
    }

    void runPendingCalibration() {
        if (!calibratePending.exchange(false)) {
            return;
        }
        if (!bmpReady) {
            return;
        }

        calibratedPressure.store(bmp.readPressure());
        Serial.printf("Calibrated pressure: %f\n", calibratedPressure.load() / 100.0f);
        sendCalibrationEvent();
    }

    void logSensorSample(const unsigned long timestamp, const SensorReading &reading) {
        sensorData.printf("%lu,%.*f,%.*f,%.*f,%.*f\n",
                          timestamp,
                          kSensorPrecision, reading.pressure,
                          kSensorPrecision, reading.altitude,
                          kSensorPrecision, reading.temperature1,
                          kSensorPrecision, reading.temperature2);

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

        server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (recording.load()) {
                request->send(409, "text/plain", "Cannot download data while recording");
                return;
            }

            if (!LittleFS.exists(kSensorDataPath)) {
                request->send(404, "text/plain", "File not found");
                return;
            }

            request->send(LittleFS, kSensorDataPath, "text/csv", true);
        });

        server.on("/calibrate-altitude", HTTP_POST, [](AsyncWebServerRequest *request) {
            if (recording.load()) {
                request->send(409, "text/plain", "Cannot calibrate while recording");
                return;
            }
            if (!bmpReady) {
                request->send(503, "text/plain", "Barometer not ready");
                return;
            }

            calibratePending.store(true);
            request->send(200);
        });
    }
} // namespace

void setup() {
    Serial.begin(115200);
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
    }
    ensureSensorDataFileExists();
    WiFi.softAP(kApSsid, kApPassword);
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
        Serial.println("Barometer init failed");
    } else {
        calibratedPressure.store(bmp.readPressure());
    }
    server.addHandler(&events);
    server.begin();
}

void loop() {
    static unsigned long lastSampleMs = 0;

    const unsigned long now = millis();

    runPendingClearData();
    if (!recording.load() && sensorData) {
        closeSensorLog();
    }
    runPendingCalibration();

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
