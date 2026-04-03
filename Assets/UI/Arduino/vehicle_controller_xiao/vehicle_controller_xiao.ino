#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// Update these credentials for your network.
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* MQTT_BROKER = "192.168.1.115";  // Change if your broker IP is different.
const int MQTT_PORT = 1883;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Change these pins to match the ESP32-XIAO wiring and motor driver.
const int LEFT_MOTOR_FORWARD = 3;
const int LEFT_MOTOR_REVERSE = 4;
const int RIGHT_MOTOR_FORWARD = 5;
const int RIGHT_MOTOR_REVERSE = 6;
const int YAW_SERVO_PIN = 9;
const int PITCH_SERVO_PIN = 10;

// Change this vehicle ID if you want the board to represent a different car.
const char* VEHICLE_ID = "0";

Servo yawServo;
Servo pitchServo;

int currentLeftSpeed = 0;
int currentRightSpeed = 0;
int currentYawAngle = 90;
int currentPitchAngle = 90;
unsigned long lastStatePublishedAt = 0;


String makeTopic(const char* subsystem, const char* direction) {
  return String("ride/vehicle/") + VEHICLE_ID + "/" + subsystem + "/" + direction;
}


void connectWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}


void publishJson(const String& topic, JsonDocument& doc) {
  char payload[256];
  serializeJson(doc, payload);
  mqttClient.publish(topic.c_str(), payload);
}


void setMotorChannel(int forwardPin, int reversePin, int speedValue) {
  int clamped = constrain(speedValue, -255, 255);

  if (clamped > 0) {
    analogWrite(forwardPin, clamped);
    analogWrite(reversePin, 0);
  } else if (clamped < 0) {
    analogWrite(forwardPin, 0);
    analogWrite(reversePin, -clamped);
  } else {
    analogWrite(forwardPin, 0);
    analogWrite(reversePin, 0);
  }
}


void applyDrive(int leftSpeed, int rightSpeed) {
  currentLeftSpeed = constrain(leftSpeed, -255, 255);
  currentRightSpeed = constrain(rightSpeed, -255, 255);

  setMotorChannel(LEFT_MOTOR_FORWARD, LEFT_MOTOR_REVERSE, currentLeftSpeed);
  setMotorChannel(RIGHT_MOTOR_FORWARD, RIGHT_MOTOR_REVERSE, currentRightSpeed);
}


void publishDriveState() {
  StaticJsonDocument<192> doc;
  doc["left_speed"] = currentLeftSpeed;
  doc["right_speed"] = currentRightSpeed;
  doc["speed"] = (currentLeftSpeed + currentRightSpeed) / 510.0;
  doc["moving"] = (currentLeftSpeed != 0 || currentRightSpeed != 0);
  publishJson(makeTopic("drive", "state"), doc);
}


void publishYawState() {
  StaticJsonDocument<96> doc;
  doc["angle"] = currentYawAngle;
  publishJson(makeTopic("servoYaw", "state"), doc);
}


void publishPitchState() {
  StaticJsonDocument<96> doc;
  doc["angle"] = currentPitchAngle;
  publishJson(makeTopic("servoPitch", "state"), doc);
}


void handleDriveCommand(JsonDocument& doc) {
  int leftSpeed = doc["left_speed"] | 0;
  int rightSpeed = doc["right_speed"] | 0;

  if (!doc["left_speed"].is<int>() || !doc["right_speed"].is<int>()) {
    float speed = doc["speed"] | 0.0;
    int pwm = constrain((int)(speed * 255.0), -255, 255);
    leftSpeed = pwm;
    rightSpeed = pwm;
  }

  applyDrive(leftSpeed, rightSpeed);
  publishDriveState();
}


void handleYawCommand(JsonDocument& doc) {
  currentYawAngle = constrain((int)(doc["angle"] | 90), 0, 180);
  yawServo.write(currentYawAngle);
  publishYawState();
}


void handlePitchCommand(JsonDocument& doc) {
  currentPitchAngle = constrain((int)(doc["angle"] | 90), 0, 180);
  pitchServo.write(currentPitchAngle);
  publishPitchState();
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    return;
  }

  String topicString = topic;

  if (topicString == makeTopic("drive", "command")) {
    handleDriveCommand(doc);
  } else if (topicString == makeTopic("servoYaw", "command")) {
    handleYawCommand(doc);
  } else if (topicString == makeTopic("servoPitch", "command")) {
    handlePitchCommand(doc);
  } else if (topicString == "ride/system/estop") {
    bool active = doc["active"] | false;
    if (active) {
      applyDrive(0, 0);
      publishDriveState();
    }
  }
}


void reconnectMqtt() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect("vehicle_controller_xiao")) {
      mqttClient.subscribe(makeTopic("drive", "command").c_str());
      mqttClient.subscribe(makeTopic("servoYaw", "command").c_str());
      mqttClient.subscribe(makeTopic("servoPitch", "command").c_str());
      mqttClient.subscribe("ride/system/estop");
      publishDriveState();
      publishYawState();
      publishPitchState();
    } else {
      delay(2000);
    }
  }
}


void setup() {
  Serial.begin(115200);

  pinMode(LEFT_MOTOR_FORWARD, OUTPUT);
  pinMode(LEFT_MOTOR_REVERSE, OUTPUT);
  pinMode(RIGHT_MOTOR_FORWARD, OUTPUT);
  pinMode(RIGHT_MOTOR_REVERSE, OUTPUT);

  yawServo.attach(YAW_SERVO_PIN);
  pitchServo.attach(PITCH_SERVO_PIN);
  yawServo.write(currentYawAngle);
  pitchServo.write(currentPitchAngle);

  connectWifi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}


void loop() {
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }

  mqttClient.loop();

  if (millis() - lastStatePublishedAt >= 1000) {
    publishDriveState();
    publishYawState();
    publishPitchState();
    lastStatePublishedAt = millis();
  }
}
