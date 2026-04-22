#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Servo.h>

// Update these credentials for your network.
const char* WIFI_SSID = "TPED";
const char* WIFI_PASSWORD = "TPEDwifi";
const char* MQTT_BROKER = "192.168.1.116";  // Change if your broker IP is different.
const int MQTT_PORT = 1883;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Update these pins to match your board and driver wiring.
const int SWITCH_SERVO_PIN = 3;
const int ROTATE_MOTOR_FORWARD_PIN = 5;   // PWM-capable on Uno R4 WiFi
const int ROTATE_MOTOR_REVERSE_PIN = 6;   // PWM-capable on Uno R4 WiFi
const int DROP_MOTOR_A_FORWARD_PIN = 9;   // PWM-capable on Uno R4 WiFi
const int DROP_MOTOR_A_REVERSE_PIN = 10;  // PWM-capable on Uno R4 WiFi
const int DROP_MOTOR_B_FORWARD_PIN = 11;  // PWM-capable on Uno R4 WiFi
const int DROP_MOTOR_B_REVERSE_PIN = A0;  // Digital direction pin (not PWM)

const bool ROTATE_MOTOR_FORWARD_PWM = true;
const bool ROTATE_MOTOR_REVERSE_PWM = true;
const bool DROP_MOTOR_A_FORWARD_PWM = true;
const bool DROP_MOTOR_A_REVERSE_PWM = true;
const bool DROP_MOTOR_B_FORWARD_PWM = true;
const bool DROP_MOTOR_B_REVERSE_PWM = false;

const int PWM_MAX_DUTY = 255;

// Change these six pins and names to match your actual IR sensor layout.
const int SENSOR_COUNT = 6;
const int IR_SENSOR_PINS[SENSOR_COUNT] = {2, 4, 7, 8, 12, 13};
const char* IR_SENSOR_NAMES[SENSOR_COUNT] = {
  "Station1",
  "Switch1",
  "Switch2",
  "Rotate1",
  "Drop1",
  "Station2"
};

int lastSensorStates[SENSOR_COUNT];
Servo switchServo;

int switchCurrentAngle = 0;
int switchTargetAngle = 0;

String rotateTarget = "stop";
bool rotateMoving = false;
unsigned long rotateMoveStartedAt = 0;
unsigned long rotateMoveDurationMs = 0;
const unsigned long ROTATE_FULL_SWEEP_TIME_MS = 3000;
int lastRotateMotorSpeed = 0;
int rotateCurrentAngle = 0;
int rotateTargetAngle = 0;

String dropTarget = "hold";
bool dropMoving = false;
unsigned long dropMoveStartedAt = 0;
const unsigned long DROP_MOVE_TIME_MS = 2200;
int lastDropMotorSpeed = 0;


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


void publishRotateState() {
  StaticJsonDocument<128> doc;
  doc["target"] = rotateTarget;
  doc["angle"] = rotateCurrentAngle;
  doc["target_angle"] = rotateTargetAngle;
  doc["moving"] = rotateMoving;
  doc["motor_speed"] = lastRotateMotorSpeed;
  publishJson("ride/actuator/rotateTrack/state", doc);
}


void publishDropState() {
  StaticJsonDocument<128> doc;
  doc["target"] = dropTarget;
  doc["moving"] = dropMoving;
  doc["motor_speed"] = lastDropMotorSpeed;
  publishJson("ride/actuator/dropTrack/state", doc);
}


void writeMotorPin(int pin, bool pwmCapable, int value) {
  int clampedValue = constrain(value, 0, PWM_MAX_DUTY);
  if (pwmCapable) {
    analogWrite(pin, clampedValue);
  } else {
    digitalWrite(pin, clampedValue > 0 ? HIGH : LOW);
  }
}


void setMotorOutput(int forwardPin, bool forwardPwmCapable, int reversePin, bool reversePwmCapable, int speedValue) {
  int clamped = constrain(speedValue, -PWM_MAX_DUTY, PWM_MAX_DUTY);

  if (clamped > 0) {
    writeMotorPin(forwardPin, forwardPwmCapable, clamped);
    writeMotorPin(reversePin, reversePwmCapable, 0);
  } else if (clamped < 0) {
    writeMotorPin(forwardPin, forwardPwmCapable, 0);
    writeMotorPin(reversePin, reversePwmCapable, -clamped);
  } else {
    writeMotorPin(forwardPin, forwardPwmCapable, 0);
    writeMotorPin(reversePin, reversePwmCapable, 0);
  }
}


void stopRotateMotor() {
  lastRotateMotorSpeed = 0;
  setMotorOutput(ROTATE_MOTOR_FORWARD_PIN, ROTATE_MOTOR_FORWARD_PWM, ROTATE_MOTOR_REVERSE_PIN, ROTATE_MOTOR_REVERSE_PWM, 0);
}


void runRotateMotor(int speedValue) {
  lastRotateMotorSpeed = constrain(speedValue, -PWM_MAX_DUTY, PWM_MAX_DUTY);
  setMotorOutput(ROTATE_MOTOR_FORWARD_PIN, ROTATE_MOTOR_FORWARD_PWM, ROTATE_MOTOR_REVERSE_PIN, ROTATE_MOTOR_REVERSE_PWM, lastRotateMotorSpeed);
}


void stopDropMotors() {
  lastDropMotorSpeed = 0;
  setMotorOutput(DROP_MOTOR_A_FORWARD_PIN, DROP_MOTOR_A_FORWARD_PWM, DROP_MOTOR_A_REVERSE_PIN, DROP_MOTOR_A_REVERSE_PWM, 0);
  setMotorOutput(DROP_MOTOR_B_FORWARD_PIN, DROP_MOTOR_B_FORWARD_PWM, DROP_MOTOR_B_REVERSE_PIN, DROP_MOTOR_B_REVERSE_PWM, 0);
}


void runDropMotors(int speedValue) {
  lastDropMotorSpeed = constrain(speedValue, -PWM_MAX_DUTY, PWM_MAX_DUTY);
  setMotorOutput(DROP_MOTOR_A_FORWARD_PIN, DROP_MOTOR_A_FORWARD_PWM, DROP_MOTOR_A_REVERSE_PIN, DROP_MOTOR_A_REVERSE_PWM, lastDropMotorSpeed);
  setMotorOutput(DROP_MOTOR_B_FORWARD_PIN, DROP_MOTOR_B_FORWARD_PWM, DROP_MOTOR_B_REVERSE_PIN, DROP_MOTOR_B_REVERSE_PWM, lastDropMotorSpeed);
}


void handleSwitchCommand(JsonDocument& doc) {
  switchTargetAngle = constrain(doc["target_angle"] | 0, 0, 180);
  publishSwitchState();
}


void handleRotateCommand(JsonDocument& doc) {
  int speedValue = doc["motor_speed"] | doc["speed"] | 180;
  rotateTarget = String((const char*)(doc["target"] | ""));

  if (doc.containsKey("target_angle")) {
    rotateTargetAngle = constrain(doc["target_angle"] | rotateCurrentAngle, 0, 180);
    int delta = rotateTargetAngle - rotateCurrentAngle;

    if (delta == 0) {
      rotateTarget = "stop";
      rotateMoving = false;
      rotateMoveDurationMs = 0;
      stopRotateMotor();
    } else {
      rotateTarget = (delta > 0) ? "cw" : "ccw";
      rotateMoving = true;
      rotateMoveStartedAt = millis();
      rotateMoveDurationMs = (unsigned long)((abs(delta) * ROTATE_FULL_SWEEP_TIME_MS) / 180);
      runRotateMotor((delta > 0) ? abs(speedValue) : -abs(speedValue));
    }

    publishRotateState();
    return;
  }

  if (rotateTarget.length() == 0) {
    rotateTarget = "stop";
  }

  if (rotateTarget == "stop") {
    rotateMoving = false;
    rotateMoveDurationMs = 0;
    rotateTargetAngle = rotateCurrentAngle;
    stopRotateMotor();
  } else if (rotateTarget == "cw") {
    rotateMoving = true;
    rotateMoveStartedAt = millis();
    rotateMoveDurationMs = ROTATE_FULL_SWEEP_TIME_MS;
    runRotateMotor(abs(speedValue));
  } else if (rotateTarget == "ccw") {
    rotateMoving = true;
    rotateMoveStartedAt = millis();
    rotateMoveDurationMs = ROTATE_FULL_SWEEP_TIME_MS;
    runRotateMotor(-abs(speedValue));
  }

  publishRotateState();
}


void handleDropCommand(JsonDocument& doc) {
  dropTarget = String((const char*)(doc["target"] | "hold"));
  int speedValue = doc["motor_speed"] | doc["speed"] | 180;

  if (dropTarget == "hold") {
    dropMoving = false;
    stopDropMotors();
  } else if (dropTarget == "bottom") {
    dropMoving = true;
    dropMoveStartedAt = millis();
    runDropMotors(-abs(speedValue));
  } else if (dropTarget == "top") {
    dropMoving = true;
    dropMoveStartedAt = millis();
    runDropMotors(abs(speedValue));
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
    handleRotateCommand(doc);
  } else if (topicString == "ride/actuator/dropTrack/command") {
    handleDropCommand(doc);
  } else if (topicString == "ride/system/estop") {
    bool active = doc["active"] | false;
    if (active) {
      rotateMoving = false;
      dropMoving = false;
      stopRotateMotor();
      stopDropMotors();
      publishRotateState();
      publishDropState();
    }
  }
}


void reconnectMqtt() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect("track_sensors")) {
      mqttClient.subscribe("ride/actuator/switchTrack/command");
      mqttClient.subscribe("ride/actuator/rotateTrack/command");
      mqttClient.subscribe("ride/actuator/dropTrack/command");
      mqttClient.subscribe("ride/system/estop");
      publishSwitchState();
      publishRotateState();
      publishDropState();
    } else {
      delay(2000);
    }
  }
}


void setup() {
  Serial.begin(115200);

  pinMode(ROTATE_MOTOR_FORWARD_PIN, OUTPUT);
  pinMode(ROTATE_MOTOR_REVERSE_PIN, OUTPUT);
  pinMode(DROP_MOTOR_A_FORWARD_PIN, OUTPUT);
  pinMode(DROP_MOTOR_A_REVERSE_PIN, OUTPUT);
  pinMode(DROP_MOTOR_B_FORWARD_PIN, OUTPUT);
  pinMode(DROP_MOTOR_B_REVERSE_PIN, OUTPUT);

  stopRotateMotor();
  stopDropMotors();

  for (int i = 0; i < SENSOR_COUNT; i++) {
    pinMode(IR_SENSOR_PINS[i], INPUT_PULLUP);  // Change if your IR modules need plain INPUT.
    lastSensorStates[i] = digitalRead(IR_SENSOR_PINS[i]);
  }

  switchServo.attach(SWITCH_SERVO_PIN);
  switchServo.write(switchCurrentAngle);

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


void updateSwitchServo() {
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
}


void updateRotateMotor() {
  if (!rotateMoving) {
    return;
  }

  if (millis() - rotateMoveStartedAt >= rotateMoveDurationMs) {
    rotateMoving = false;
    rotateCurrentAngle = rotateTargetAngle;
    stopRotateMotor();
    publishRotateState();
  }
}


void updateDropMotors() {
  if (!dropMoving) {
    return;
  }

  if (millis() - dropMoveStartedAt >= DROP_MOVE_TIME_MS) {
    dropMoving = false;
    stopDropMotors();
    publishDropState();
  }
}


void loop() {
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }

  mqttClient.loop();
  updateSensors();
  updateSwitchServo();
  updateRotateMotor();
  updateDropMotors();
}
