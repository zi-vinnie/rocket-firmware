#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <atomic>
#include <ArduinoJson.h>
#include "Adafruit_BMP085.h"
#include "lights.h"

namespace {
    constexpr const char *kApSsid = "Space-Exe-Rocket";
    constexpr const char *kApPassword = "space2200ft";
    constexpr const char *kSensorDataPath = "/data/sensors.csv";
    constexpr const char *kApogeePath = "/data/apogee.csv";
    constexpr unsigned long kSensorIntervalMs = 100;
    constexpr unsigned long kFlushIntervalMs = 1000;
    constexpr float kMockPressureHpa = 1000.00f;
    constexpr int kSensorPrecision = 2;
    float apogee = 0.0f;

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
    File sensorFile;
    File apogeeFile;

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
        doc["apogee"] = apogee;
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
        if (sensorFile) {
            Serial.println("Closing sensor data file");
            sensorFile.flush();
            sensorFile.close();
            sensorFile = File();
        }
        if (apogeeFile) {
            Serial.println("Closing apogee file");
            apogeeFile.flush();
            apogeeFile.close();
            apogeeFile = File();
        }
    }

    void ensureSensorDataFileExists() {
        if (LittleFS.exists(kSensorDataPath)) {
            return;
        }
        File file = LittleFS.open(kSensorDataPath, FILE_WRITE, true);
        if (!file) {
            Serial.println("Failed to create sensor data file");
            return;
        }
        file.printf("timestamp,pressure,altitude,temperature1,temperature2\n");
        file.close();
    }

    void ensureApogeeFileExists() {
        if (LittleFS.exists(kApogeePath)) {
            return;
        }
        File file = LittleFS.open(kApogeePath, FILE_WRITE, true);
        if (!file) {
            Serial.println("Failed to create apogee file");
            return;
        }
        file.printf("timestamp,apogee\n");
        file.close();
    }

    void runPendingClearData() {
        if (!clearDataPending.exchange(false)) {
            return;
        }
        Serial.println("Clearing data");
        closeSensorLog();
        if (LittleFS.exists(kSensorDataPath)) {
            LittleFS.remove(kSensorDataPath);
        }
        if (LittleFS.exists(kApogeePath)) {
            LittleFS.remove(kApogeePath);
        }
        ensureSensorDataFileExists();
        ensureApogeeFileExists();
        apogee = 0.0f;
    }

    void runPendingCalibration() {
        if (!calibratePending.exchange(false)) {
            return;
        }
        Serial.println("Calibrating altitude");
        if (!bmpReady) {
            Serial.println("Could not calibrate altitude, barometer not ready");
            return;
        }
        calibratedPressure.store(bmp.readPressure());
        Serial.printf("Calibrated pressure: %f\n", calibratedPressure.load() / 100.0f);
        sendCalibrationEvent();
    }

    void logSensorSample(const unsigned long timestamp, const SensorReading &reading) {
        sensorFile.printf("%lu,%.*f,%.*f,%.*f,%.*f\n",
                          timestamp,
                          kSensorPrecision, reading.pressure,
                          kSensorPrecision, reading.altitude,
                          kSensorPrecision, reading.temperature1,
                          kSensorPrecision, reading.temperature2);

        if (timestamp - lastFlush >= kFlushIntervalMs) {
            sensorFile.flush();
        }
    }

    void logApogee(const unsigned long timestamp) {
        if (timestamp - lastFlush >= kFlushIntervalMs) {
            apogeeFile.printf("%lu,%.*f\n", timestamp, kSensorPrecision, apogee);
            apogeeFile.flush();
        }
    }

    void setupServer() {
        server.serveStatic("/", LittleFS, "/website/").setDefaultFile("index.html");

        server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(LittleFS, "/website/favicon.ico", "image/x-icon");
        });

        server.on("/toggle-recording", HTTP_POST, [](AsyncWebServerRequest *request) {
            const bool nowRecording = !recording.load();
            recording.store(nowRecording);
            Serial.println(nowRecording ? "Recording started" : "Recording stopped");
            sendRecordingEvent();
            request->send(200);
        });

        server.on("/clear-data", HTTP_POST, [](AsyncWebServerRequest *request) {
            if (recording.load()) {
                Serial.println("Rejected clear-data: recording in progress");
                request->send(409, "text/plain", "Cannot clear while recording");
                return;
            }

            clearDataPending.store(true);
            request->send(200);
        });

        server.on("/download-sensor-data", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (recording.load()) {
                Serial.println("Rejected download-sensor-data: recording in progress");
                request->send(409, "text/plain", "Cannot download sensor data while recording");
                return;
            }

            if (!LittleFS.exists(kSensorDataPath)) {
                Serial.println("Rejected download-sensor-data: file not found");
                request->send(404, "text/plain", "File not found");
                return;
            }

            request->send(LittleFS, kSensorDataPath, "text/csv", true);
        });

        server.on("/download-apogee", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (recording.load()) {
                Serial.println("Rejected download-apogee: recording in progress");
                request->send(409, "text/plain", "Cannot download apogee while recording");
                return;
            }

            if (!LittleFS.exists(kApogeePath)) {
                Serial.println("Rejected download-apogee: file not found");
                request->send(404, "text/plain", "File not found");
                return;
            }

            request->send(LittleFS, kApogeePath, "text/csv", true);
        });

        server.on("/calibrate-altitude", HTTP_POST, [](AsyncWebServerRequest *request) {
            if (recording.load()) {
                Serial.println("Rejected calibrate-altitude: recording in progress");
                request->send(409, "text/plain", "Cannot calibrate while recording");
                return;
            }
            if (!bmpReady) {
                Serial.println("Rejected calibrate-altitude: barometer not ready");
                request->send(503, "text/plain", "Barometer not ready");
                return;
            }

            calibratePending.store(true);
            request->send(200);
        });

        events.onConnect([](AsyncEventSourceClient *client) {
            if (client->lastId()) {
                Serial.printf("Client reconnecting, last event id: %u\n", client->lastId());
            } else {
                Serial.println("SSE client connected");
            }

            client->send(recordingStateText(), "recording", millis(), 10000);
        });

        server.addHandler(&events);
    }
} // namespace

void setup() {
    Serial.begin(115200);
    setup_rgb_lights();
    // This is added since A4 (SDA) and A3 accidentally been shorted together
    pinMode(A3, INPUT);
    pinMode(A1, INPUT);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    }

    ensureSensorDataFileExists();
    ensureApogeeFileExists();

    // const float readApogee = 1482.00f; // TODO: read from file
    // Serial.printf("Displaying apogee of %.*fm\n", kSensorPrecision, readApogee);
    // display_apogee(readApogee, kSensorPrecision);

    WiFiClass::mode(WIFI_AP);
    WiFi.softAP(kApSsid, kApPassword);
    Serial.printf("WiFi AP started: %s at %s\n", kApSsid, WiFi.softAPIP().toString().c_str());

    setupServer();

    bmpReady = bmp.begin();
    if (!bmpReady) {
        Serial.println("Barometer init failed, using mock sensor data");
    } else {
        calibratedPressure.store(bmp.readPressure());
    }
    server.begin();
}

void loop() {
    static unsigned long lastSampleMs = 0;

    const unsigned long now = millis();

    runPendingClearData();
    runPendingCalibration();
    if (!recording.load()) {
        closeSensorLog();
    }

    if (now - lastSampleMs < kSensorIntervalMs) {
        return;
    }
    lastSampleMs = now;

    const SensorReading reading = readSensors();
    if (reading.altitude > apogee) {
        apogee = reading.altitude;
    }
    sendSensorEvent(reading);

    if (recording.load()) {
        if (!sensorFile || !apogeeFile) {
            if (!sensorFile) {
                Serial.println("Opening sensor file for writing since recording started");
                sensorFile = LittleFS.open(kSensorDataPath, FILE_APPEND);
            }
            if (!apogeeFile) {
                Serial.println("Opening apogee file for writing since recording started");
                apogeeFile = LittleFS.open(kApogeePath, FILE_APPEND);
            }
            if (!sensorFile || !apogeeFile) {
                Serial.println("Failed to open file for writing");
                recording.store(false);
                sendRecordingEvent();
            } else {
                lastFlush = now;
                logSensorSample(now, reading);
                logApogee(now);
            }
        } else {
            logSensorSample(now, reading);
            logApogee(now);
        }
        if (now - lastFlush >= kFlushIntervalMs) {
            lastFlush = now;
        }
    }
}
