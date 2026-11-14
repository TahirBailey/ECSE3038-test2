
#include <Arduino.h>
#include <DHT.h>
#include "secrets.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Pin configuration
#define DHTPIN 15
#define DHTTYPE DHT22
#define FAN_LED 2

// WiFi settings
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

// API settings
#define API_URL "http://host.wokwi.internal:8000/readings"

// MQTT broker settings
#define MQTT_BROKER "135.232.203.144"
#define MQTT_PORT 1883
#define MQTT_NAME "tahirbailey"

// MQTT topics
#define TOPIC_STATUS "ecse3038/" MQTT_NAME "/status"
#define TOPIC_SETTINGS "ecse3038/" MQTT_NAME "/fan/settings"
#define TOPIC_STATE "ecse3038/" MQTT_NAME "/fan/state"

// Create sensor and network clients
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Fan settings
String fanMode = "off";
float maxHumidity = 65.0;
bool fanState = false;

float currentHumidity = NAN;

unsigned long lastReading = 0;
unsigned long lastMQTTAttempt = 0;

// Connect to WiFi
void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected!");

    Serial.print("ESP32 MAC Address: ");
    Serial.println(WiFi.macAddress());
}

// Send humidity reading to API
void sendReading(float humidity) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        http.begin(API_URL);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-API-Key", DEVICE_KEY);

        String body = "{\"mac\":\"" + WiFi.macAddress() +
                      "\",\"humidity\":" + String(humidity, 1) + "}";

        int responseCode = http.POST(body);

        Serial.print("API response: ");
        Serial.println(responseCode);

        http.end();
    }
}

// Update fan LED and publish state when it changes
void updateFanState() {
    bool newState = false;

    if (fanMode == "on") {
        newState = true;
    }
    else if (fanMode == "off") {
        newState = false;
    }
    else if (fanMode == "auto") {
        if (!isnan(currentHumidity)) {
            newState = currentHumidity > maxHumidity;
        }
    }

    if (newState != fanState) {
        fanState = newState;

        digitalWrite(FAN_LED, fanState ? HIGH : LOW);

        const char* stateMessage = fanState ? "on" : "off";

        Serial.print("Fan state changed: ");
        Serial.println(stateMessage);

        if (mqttClient.connected()) {
            mqttClient.publish(TOPIC_STATE, stateMessage, true);
        }
    }
}

// Receive settings from MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("MQTT settings received");

    String message = "";

    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.println("Invalid JSON. Keeping previous settings.");
        return;
    }

    if (!doc["mode"].is<const char*>()) {
        Serial.println("Missing or invalid mode.");
        return;
    }

    if (!doc["max_humidity"].is<float>()) {
        Serial.println("Missing or invalid max_humidity.");
        return;
    }

    String newMode = doc["mode"].as<String>();
    float newMaxHumidity = doc["max_humidity"].as<float>();

    if (newMode != "auto" &&
        newMode != "on" &&
        newMode != "off") {
        Serial.println("Invalid fan mode.");
        return;
    }

    if (newMaxHumidity < 0 || newMaxHumidity > 100) {
        Serial.println("Invalid humidity limit.");
        return;
    }

    // Apply valid settings
    fanMode = newMode;
    maxHumidity = newMaxHumidity;

    Serial.print("Fan mode: ");
    Serial.println(fanMode);

    Serial.print("Maximum humidity: ");
    Serial.println(maxHumidity);

    updateFanState();
}

// Connect to MQTT broker
void connectMQTT() {
    if (mqttClient.connected()) {
        return;
    }

    if (millis() - lastMQTTAttempt < 5000) {
        return;
    }

    lastMQTTAttempt = millis();

    Serial.println("Connecting to MQTT...");

    String clientId = String(MQTT_NAME) + "-" + WiFi.macAddress();

    if (mqttClient.connect(
        clientId.c_str(),
        TOPIC_STATUS,
        1,
        true,
        "offline"
    )) {
        Serial.println("MQTT connected!");

        mqttClient.publish(TOPIC_STATUS, "online", true);

        mqttClient.subscribe(TOPIC_SETTINGS);

        Serial.println("Subscribed to fan settings.");
    }
    else {
        Serial.print("MQTT connection failed: ");
        Serial.println(mqttClient.state());
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(FAN_LED, OUTPUT);
    digitalWrite(FAN_LED, LOW);

    dht.begin();

    Serial.println("Bathroom Fan Starting...");

    connectWiFi();

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
}

void loop() {
    // Maintain MQTT connection
    connectMQTT();
    mqttClient.loop();

    // Read humidity every 10 seconds
    if (millis() - lastReading >= 10000) {
        lastReading = millis();

        float humidity = dht.readHumidity();

        if (isnan(humidity)) {
            Serial.println("DHT22 reading failed");
        }
        else {
            currentHumidity = humidity;

            Serial.print("Humidity: ");
            Serial.print(humidity);
            Serial.println("%");

            sendReading(humidity);

            updateFanState();
        }
    }
}
