#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

const char* ssid = "TPED";
const char* password = "TPEDwifi";
const char* mqtt_server = "192.168.1.116";

WiFiClient espClient;
PubSubClient client(espClient);

// ✅ PIN DEFINITIONS
const int LEFT_MOTOR_FORWARD = D7;
const int LEFT_MOTOR_REVERSE = D8;
const int RIGHT_MOTOR_FORWARD = D5;
const int RIGHT_MOTOR_REVERSE = D6;

const int YAW_SERVO_PIN = D2;

Servo yawServo;

// ✅ PWM setup for Motors
const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8;

const int CH_LEFT_FWD = 0;
const int CH_LEFT_REV = 1;
const int CH_RIGHT_FWD = 2;
const int CH_RIGHT_REV = 3;

unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 2000;

void setup_wifi() {
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void setMotorChannel(int chFwd, int chRev, int speedValue) {
  int clamped = constrain(speedValue, -255, 255);

  if (clamped > 0) {
    ledcWrite(chFwd, clamped);
    ledcWrite(chRev, 0);
  } else if (clamped < 0) {
    ledcWrite(chFwd, 0);
    ledcWrite(chRev, -clamped);
  } else {
    ledcWrite(chFwd, 0);
    ledcWrite(chRev, 0);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message on topic: ");
  Serial.println(topic);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("JSON Parse Error: ");
    Serial.println(error.c_str());
    return;
  }

  String topicStr = String(topic);

  if (topicStr == "ride/vehicle/0/drive/command") {
    int leftSpeed = 0;
    int rightSpeed = 0;

    if (doc.containsKey("left_speed") && doc.containsKey("right_speed")) {
      leftSpeed = doc["left_speed"] | 0;
      rightSpeed = doc["right_speed"] | 0;
    } else if (doc.containsKey("speed")) {
      leftSpeed = rightSpeed = doc["speed"] | 0;
    }

    setMotorChannel(CH_LEFT_FWD, CH_LEFT_REV, leftSpeed);
    setMotorChannel(CH_RIGHT_FWD, CH_RIGHT_REV, rightSpeed);
  }
  else if (topicStr == "ride/vehicle/0/servoYaw/command") {
    int angle = constrain(doc["angle"] | 90, 0, 180);
    Serial.printf("Yaw -> %d\n", angle);
    yawServo.write(angle);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("RideVehicleESP")) {
      Serial.println("connected");
      client.subscribe("ride/vehicle/0/drive/command");
      client.subscribe("ride/vehicle/0/servoYaw/command");
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Setup Motor PWM
  ledcAttach(LEFT_MOTOR_FORWARD, PWM_FREQ, CH_LEFT_FWD);
  ledcAttach(LEFT_MOTOR_REVERSE, PWM_FREQ, CH_LEFT_REV);
  ledcAttach(RIGHT_MOTOR_FORWARD, PWM_FREQ, CH_RIGHT_FWD);
  ledcAttach(RIGHT_MOTOR_REVERSE, PWM_FREQ, CH_RIGHT_REV);

  setMotorChannel(CH_LEFT_FWD, CH_LEFT_REV, 0);
  setMotorChannel(CH_RIGHT_FWD, CH_RIGHT_REV, 0);

  // Setup Servo - Using Timer 3 to avoid motor PWM conflict (0-2)
  ESP32PWM::allocateTimer(3);
  yawServo.setPeriodHertz(50);
  yawServo.attach(YAW_SERVO_PIN, 500, 2400);
  yawServo.write(90);

  Serial.println("Setup complete");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastPingTime > PING_INTERVAL) {
    lastPingTime = now;
    client.publish("ride/vehicle/0/ping", "{\"status\":\"ok\"}");
  }
}
