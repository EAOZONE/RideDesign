#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

const char* ssid = "TPED";
const char* password = "TPEDwifi";
const char* mqtt_server = "192.168.1.115";

WiFiClient espClient;
PubSubClient client(espClient);

// Adjust these pins to match your ESP32 wiring and motor driver
const int LEFT_MOTOR_FORWARD = 25;
const int LEFT_MOTOR_REVERSE = 26;
const int RIGHT_MOTOR_FORWARD = 27;
const int RIGHT_MOTOR_REVERSE = 14;

const int YAW_SERVO_PIN = 32;
const int PITCH_SERVO_PIN = 33;

Servo yawServo;
Servo pitchServo;

unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 2000;

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
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

void callback(char* topic, byte* payload, unsigned int length) {
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
      float s = doc["speed"] | 0.0f;
      int pwm = s * 255.0f;
      leftSpeed = pwm;
      rightSpeed = pwm;
    } else if (doc.containsKey("pwm")) {
      int pwm = doc["pwm"] | 0;
      leftSpeed = pwm;
      rightSpeed = pwm;
    }

    setMotorChannel(LEFT_MOTOR_FORWARD, LEFT_MOTOR_REVERSE, leftSpeed);
    setMotorChannel(RIGHT_MOTOR_FORWARD, RIGHT_MOTOR_REVERSE, rightSpeed);
  } 
  else if (topicStr == "ride/vehicle/0/servoYaw/command") {
    if (doc.containsKey("angle")) {
      int angle = doc["angle"] | 90;
      yawServo.write(constrain(angle, 0, 180));
    }
  } 
  else if (topicStr == "ride/vehicle/0/servoPitch/command") {
    if (doc.containsKey("angle")) {
      int angle = doc["angle"] | 90;
      pitchServo.write(constrain(angle, 0, 180));
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("RideVehicleESP")) {
      Serial.println("connected");
      client.subscribe("ride/vehicle/0/drive/command");
      client.subscribe("ride/vehicle/0/servoYaw/command");
      client.subscribe("ride/vehicle/0/servoPitch/command");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Setup motor pins
  pinMode(LEFT_MOTOR_FORWARD, OUTPUT);
  pinMode(LEFT_MOTOR_REVERSE, OUTPUT);
  pinMode(RIGHT_MOTOR_FORWARD, OUTPUT);
  pinMode(RIGHT_MOTOR_REVERSE, OUTPUT);

  // Initial motor state (stopped)
  setMotorChannel(LEFT_MOTOR_FORWARD, LEFT_MOTOR_REVERSE, 0);
  setMotorChannel(RIGHT_MOTOR_FORWARD, RIGHT_MOTOR_REVERSE, 0);

  // Setup servos
  yawServo.attach(YAW_SERVO_PIN);
  pitchServo.attach(PITCH_SERVO_PIN);
  yawServo.write(90);
  pitchServo.write(90);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Ping connection
  unsigned long now = millis();
  if (now - lastPingTime > PING_INTERVAL) {
    lastPingTime = now;
    client.publish("ride/vehicle/0/ping", "{\"status\":\"ok\"}");
  }
}
