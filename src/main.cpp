#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>

auto ssid = "Space-Exe-Rocket";
auto password = "space2200ft";
auto recording = false;
volatile bool recordingChanged = false;

AsyncWebServer server(80);
AsyncEventSource events("/events");

void setupServer() {
    server.serveStatic("/", LittleFS, "/website/").setDefaultFile("index.html");
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/website/favicon.ico", "image/x-icon");
    });
    server.on("/toggle-recording", HTTP_POST, [](AsyncWebServerRequest *request) {
        recordingChanged = true;
        request->send(200, "text/plain", String(recording ? "true" : "false").c_str());
    });
}

void setup() {
    Serial.begin(115200);
    WiFi.softAP(ssid, password);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(A0, INPUT);
    pinMode(A1, INPUT);

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
    }

    setupServer();

    events.onConnect([](AsyncEventSourceClient *client) {
        // Fires each time a client connects or reconnects.
        // Send initial state immediately so the UI doesn't have to
        // wait for the next scheduled broadcast.

        if (client->lastId()) {
            Serial.printf("Client reconnecting, last event id: %u\n",
                          client->lastId());
        }

        // send(data, event name, id, reconnect interval ms)
        client->send(String(recording).c_str(), "recording", millis(), 10000);
    });

    server.addHandler(&events);

    server.begin();
}

void loop() {
    static unsigned long lastSend = 0;
    const int pressure = 1000;
    const int temperature1 = analogRead(A0);
    const int temperature2 = analogRead(A1);

    if (millis() - lastSend >= 100) {
        lastSend = millis();

        if (recordingChanged) {
            recording = !recording;
            recordingChanged = false;
            events.send(String(recording ? "true" : "false").c_str(), "recording", millis());
        }

        // Named event with JSON — useful when sending multiple values
        const String json = "{\"pressure\":" + String(pressure) + ",\"temperature1\":" + String(temperature1) + ",\"temperature2\":" + String(temperature2) + "}";
        events.send(json.c_str(), "sensor", millis());
    }
}
