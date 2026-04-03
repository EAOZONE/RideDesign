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

// Change these pins to match your board and driver wiring.
const int DROP_MOTOR_A_FORWARD = 3;
const int DROP_MOTOR_A_REVERSE = 4;
const int DROP_MOTOR_B_FORWARD = 5;
const int DROP_MOTOR_B_REVERSE = 6;
const int SWITCH_SERVO_PIN = 9;
const int TURNTABLE_SERVO_PIN = 10;

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

// Adjust these angles to match your mechanical setup.
const int SWITCH_STRAIGHT_ANGLE = 0;
const int SWITCH_DIVERGE_ANGLE = 90;
const int TURNTABLE_HOME_ANGLE = 0;
const int TURNTABLE_ROTATED_ANGLE = 90;

Servo switchServo;
Servo turntableServo;

int lastSensorStates[SENSOR_COUNT];
int switchCurrentAngle = SWITCH_STRAIGHT_ANGLE;
int switchTargetAngle = SWITCH_STRAIGHT_ANGLE;
int turntableCurrentAngle = TURNTABLE_HOME_ANGLE;
int turntableTargetAngle = TURNTABLE_HOME_ANGLE;
String dropTarget = "top";
bool dropMoving = false;
unsigned long dropMoveStartedAt = 0;
const unsigned long DROP_MOVE_TIME_MS = 2200;  // Adjust to match your lift/drop travel time.
int lastDropMotorASpeed = 0;
int lastDropMotorBSpeed = 0;


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


void publishSwitchState() {
  StaticJsonDocument<128> doc;
  doc["angle"] = switchCurrentAngle;
  doc["target_angle"] = switchTargetAngle;
  doc["moving"] = (switchCurrentAngle != switchTargetAngle);
  publishJson("ride/actuator/switchTrack/state", doc);
}


void publishTurntableState() {
  StaticJsonDocument<128> doc;
  doc["angle"] = turntableCurrentAngle;
  doc["target_angle"] = turntableTargetAngle;
  doc["moving"] = (turntableCurrentAngle != turntableTargetAngle);
  publishJson("ride/actuator/rotateTrack/state", doc);
}


void publishDropState() {
  StaticJsonDocument<192> doc;
  doc["target"] = dropTarget;
  doc["moving"] = dropMoving;
  doc["motor_a_speed"] = lastDropMotorASpeed;
  doc["motor_b_speed"] = lastDropMotorBSpeed;
  publishJson("ride/actuator/dropTrack/state", doc);
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


void runDropMotors(int motorASpeed, int motorBSpeed) {
  lastDropMotorASpeed = constrain(motorASpeed, -255, 255);
  lastDropMotorBSpeed = constrain(motorBSpeed, -255, 255);

  setMotorChannel(DROP_MOTOR_A_FORWARD, DROP_MOTOR_A_REVERSE, lastDropMotorASpeed);
  setMotorChannel(DROP_MOTOR_B_FORWARD, DROP_MOTOR_B_REVERSE, lastDropMotorBSpeed);
}


void stopDropMotors() {
  runDropMotors(0, 0);
}


void handleSwitchCommand(JsonDocument& doc) {
  switchTargetAngle = doc["target_angle"] | SWITCH_STRAIGHT_ANGLE;
  switchTargetAngle = constrain(switchTargetAngle, 0, 180);
  publishSwitchState();
}


void handleTurntableCommand(JsonDocument& doc) {
  turntableTargetAngle = doc["target_angle"] | TURNTABLE_HOME_ANGLE;
  turntableTargetAngle = constrain(turntableTargetAngle, 0, 180);
  publishTurntableState();
}


void handleDropCommand(JsonDocument& doc) {
  dropTarget = String((const char*)(doc["target"] | "hold"));
  int motorASpeed = doc["motor_a_speed"] | 0;
  int motorBSpeed = doc["motor_b_speed"] | 0;

  if (dropTarget == "hold") {
    dropMoving = false;
    stopDropMotors();
  } else if (dropTarget == "bottom") {
    dropMoving = true;
    runDropMotors(-abs(motorASpeed), -abs(motorBSpeed));
    dropMoveStartedAt = millis();
  } else if (dropTarget == "top") {
    dropMoving = true;
    runDropMotors(abs(motorASpeed), abs(motorBSpeed));
    dropMoveStartedAt = millis();
  }

  publishDropState();
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    return;
  }

  String topicString = topic;

  if (topicString == "ride/actuator/switchTrack/command") {
    handleSwitchCommand(doc);
  } else if (topicString == "ride/actuator/rotateTrack/command") {
    handleTurntableCommand(doc);
  } else if (topicString == "ride/actuator/dropTrack/command") {
    handleDropCommand(doc);
  } else if (topicString == "ride/system/estop") {
    bool active = doc["active"] | false;
    if (active) {
      stopDropMotors();
      dropMoving = false;
      publishDropState();
    }
  }
}


void reconnectMqtt() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect("track_controller")) {
      mqttClient.subscribe("ride/actuator/switchTrack/command");
      mqttClient.subscribe("ride/actuator/rotateTrack/command");
      mqttClient.subscribe("ride/actuator/dropTrack/command");
      mqttClient.subscribe("ride/system/estop");
      publishSwitchState();
      publishTurntableState();
      publishDropState();
    } else {
      delay(2000);
    }
  }
}


void setup() {
  Serial.begin(115200);

  pinMode(DROP_MOTOR_A_FORWARD, OUTPUT);
  pinMode(DROP_MOTOR_A_REVERSE, OUTPUT);
  pinMode(DROP_MOTOR_B_FORWARD, OUTPUT);
  pinMode(DROP_MOTOR_B_REVERSE, OUTPUT);

  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(IR_SENSOR_PINS[i], INPUT_PULLUP);  // Change if your IR modules need plain INPUT.
    lastSensorStates[i] = digitalRead(IR_SENSOR_PINS[i]);
  }

  switchServo.attach(SWITCH_SERVO_PIN);
  turntableServo.attach(TURNTABLE_SERVO_PIN);
  switchServo.write(switchCurrentAngle);
  turntableServo.write(turntableCurrentAngle);

  connectWifi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
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


void updateServoMotion() {
  static unsigned long lastServoStepAt = 0;

  if (millis() - lastServoStepAt < 20) {
    return;
  }

  lastServoStepAt = millis();

  if (switchCurrentAngle < switchTargetAngle) {
    switchCurrentAngle++;
    switchServo.write(switchCurrentAngle);
    publishSwitchState();
  } else if (switchCurrentAngle > switchTargetAngle) {
    switchCurrentAngle--;
    switchServo.write(switchCurrentAngle);
    publishSwitchState();
  }

  if (turntableCurrentAngle < turntableTargetAngle) {
    turntableCurrentAngle++;
    turntableServo.write(turntableCurrentAngle);
    publishTurntableState();
  } else if (turntableCurrentAngle > turntableTargetAngle) {
    turntableCurrentAngle--;
    turntableServo.write(turntableCurrentAngle);
    publishTurntableState();
  }
}


void updateDropMotion() {
  if (!dropMoving) {
    return;
  }

  if (millis() - dropMoveStartedAt >= DROP_MOVE_TIME_MS) {
    stopDropMotors();
    dropMoving = false;
    publishDropState();
  }
}


void loop() {
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }

  mqttClient.loop();
  updateSensors();
  updateServoMotion();
  updateDropMotion();
}
