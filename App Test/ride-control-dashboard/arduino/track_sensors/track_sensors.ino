#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Update these credentials for your network.
const char* WIFI_SSID = "TPED";
const char* WIFI_PASSWORD = "TPEDwifi";
const char* MQTT_BROKER = "192.168.1.115";  // Change if your broker IP is different.
const int MQTT_PORT = 1883;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Change these six pins and names to match your actual IR sensor layout.
const int SENSOR_COUNT = 6;
const int IR_SENSOR_PINS[SENSOR_COUNT] = {13, 14, 15, 16, 17, 18};
const char* IR_SENSOR_NAMES[SENSOR_COUNT] = {
  "Station1",
  "Switch1",
  "Switch2",
  "Rotate1",
  "Drop1",
  "Station2"
};

int lastSensorStates[SENSOR_COUNT];


void connectWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}


void publishJson(const char* topic, JsonDocument& doc) {
  char payload[256];
  serializeJson(doc, payload);
  mqttClient.publish(topic, payload);
}


void publishSensorState(const char* sensorName, int state) {
  StaticJsonDocument<128> doc;
  doc["sensor_id"] = sensorName;
  doc["state"] = state;
  publishJson((String("ride/sensor/") + sensorName + "/state").c_str(), doc);
}


void reconnectMqtt() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect("track_sensors")) {
      // Successfully connected
    } else {
      delay(2000);
    }
  }
}


void setup() {
  Serial.begin(115200);

  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(IR_SENSOR_PINS[i], INPUT_PULLUP);  // Change if your IR modules need plain INPUT.
    lastSensorStates[i] = digitalRead(IR_SENSOR_PINS[i]);
  }

  connectWifi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
}


void updateSensors() {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    int state = digitalRead(IR_SENSOR_PINS[i]);

    if (state != lastSensorStates[i]) {
      // The payload reports 1 when the beam is broken. Invert here if your sensors behave differently.
      int activeState = (state == LOW) ? 1 : 0;
      publishSensorState(IR_SENSOR_NAMES[i], activeState);
      lastSensorStates[i] = state;
    }
  }
}


void loop() {
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }

  mqttClient.loop();
  updateSensors();
}
