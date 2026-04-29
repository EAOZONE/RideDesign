/*
=========================================================
UNO R4 WIFI + PCA9685 + MQTT
UPDATED SYSTEM

CHANGE:
Turntable is now a SERVO (0° to 180°)
instead of motor timer rotation.

PCA CHANNELS:
CH0 = Switch Track Servo
CH8 = Turntable Servo

MQTT:
ride/actuator/rotateTrack/command
{"target_angle":0}
{"target_angle":180}

=========================================================
*/

#include <WiFiS3.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

const char* WIFI_SSID     = "TPED";
const char* WIFI_PASSWORD = "TPEDwifi";

const char* MQTT_BROKER = "192.168.1.116";
const int MQTT_PORT = 1883;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// =====================================================
// CHANNELS
// =====================================================

const int SWITCH_SERVO = 0;
const int TURN_SERVO   = 10;

// Drop motors
const int DL_FWD = 3;
const int DL_REV = 4;
const int DR_FWD = 1;
const int DR_REV = 2;

// =====================================================
// SERVO SETTINGS
// =====================================================

// Switch Track Calibration
const int SWITCH_MIN = 110;
const int SWITCH_MAX = 900;

// Turntable Calibration
const int TURN_MIN = 120;
const int TURN_MAX = 620;

// switch track
int switchCurrent = 0;
int switchTarget  = 0;

// turntable
int turnCurrent = 0;
int turnTarget  = 0;

// =====================================================
// DROP ENCODER
// =====================================================

const int ENC_A = 2;
const int ENC_B = 3;

volatile long dropCount = 0;
volatile int lastEncoded = 0;

// =====================================================
// SENSOR PINS
// =====================================================

const int SENSOR_STATION      = 4;
const int SENSOR_SWITCH1      = 5;
const int SENSOR_SWITCH2      = 6;
const int SENSOR_ROTATE1      = 7;
const int SENSOR_DROP2_TOP    = 10;
const int SENSOR_DROP1_BOTTOM = 11;

bool lastStation = HIGH;
bool lastSwitch1 = HIGH;
bool lastSwitch2 = HIGH;
bool lastRotate1 = HIGH;
bool lastDropTop = HIGH;
bool lastDropBot = HIGH;

// =====================================================
// DROP SETTINGS
// =====================================================

const long DROP_TOP    = 13400;
const long DROP_BOTTOM = 7400;
const int DROP_SPEED   = 4095;
const int DROP_TOL     = 10;

// =====================================================
// HELPERS
// =====================================================

void setPWM(int ch, int val)
{
  val = constrain(val, 0, 4095);
  pwm.setPWM(ch, 0, val);
}

int angleToPulse(int angle, int minPulse, int maxPulse)
{
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, minPulse, maxPulse);
}

void setServoChannel(int ch, int angle)
{
  int pulse;
  if (ch == SWITCH_SERVO) {
    pulse = angleToPulse(angle, SWITCH_MIN, SWITCH_MAX);
  } else if (ch == TURN_SERVO) {
    pulse = angleToPulse(angle, TURN_MIN, TURN_MAX);
  } else {
    // Default fallback
    pulse = angleToPulse(angle, 120, 620);
  }
  pwm.setPWM(ch, 0, pulse);
}

// =====================================================
// STOP
// =====================================================

void stopDrop()
{
  setPWM(DL_FWD, 0);
  setPWM(DL_REV, 0);
  setPWM(DR_FWD, 0);
  setPWM(DR_REV, 0);
}

// =====================================================
// DROP MOTOR
// =====================================================

void moveDropUp()
{
  setPWM(DL_FWD, 0);
  setPWM(DL_REV, DROP_SPEED);

  setPWM(DR_FWD, DROP_SPEED);
  setPWM(DR_REV, 0);
}

void moveDropDown()
{
  setPWM(DL_FWD, DROP_SPEED);
  setPWM(DL_REV, 0);

  setPWM(DR_FWD, 0);
  setPWM(DR_REV, DROP_SPEED);
}

// =====================================================
// DROP ENCODER
// =====================================================

void isrDrop()
{
  int MSB = digitalRead(ENC_A);
  int LSB = digitalRead(ENC_B);

  int encoded = (MSB << 1) | LSB;
  int sum = (lastEncoded << 2) | encoded;

  if (sum == 0b1101 || sum == 0b0100 ||
      sum == 0b0010 || sum == 0b1011)
    dropCount++;

  if (sum == 0b1110 || sum == 0b0111 ||
      sum == 0b0001 || sum == 0b1000)
    dropCount--;

  lastEncoded = encoded;
}

long getDropCount()
{
  noInterrupts();
  long v = dropCount;
  interrupts();
  return v;
}

// =====================================================
// DROP POSITION
// =====================================================

void goDropTo(long target)
{
  while (1)
  {
    mqttClient.loop();

    long pos = getDropCount();
    long error = target - pos;

    if (abs(error) <= DROP_TOL)
    {
      stopDrop();
      return;
    }

    if (error > 0)
      moveDropUp();
    else
      moveDropDown();

    delay(5);
  }
}

// =====================================================
// MQTT CALLBACK
// =====================================================

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  String t = topic;

  Serial.print("MQTT [");
  Serial.print(t);
  Serial.print("] Payload: ");
  for (int i = 0; i < length; i++) Serial.print((char)payload[i]);
  Serial.println();

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);

  if (t == "ride/system/estop")
  {
    Serial.println("!!! E-STOP RECEIVED !!!");
    stopDrop();
    return;
  }

  // switch track servo
  if (t == "ride/actuator/switchTrack/command")
  {
    switchTarget = constrain(doc["target_angle"] | 0, 0, 180);
    Serial.print("Switch Track Target -> ");
    Serial.println(switchTarget);
  }

  // turntable servo
  else if (t == "ride/actuator/rotateTrack/command")
  {
    turnTarget = constrain(doc["target_angle"] | 0, 0, 180);
    Serial.print("Turntable Target -> ");
    Serial.println(turnTarget);
  }

  // drop track
  else if (t == "ride/actuator/dropTrack/command")
  {
    String cmd = doc["target"] | "stop";
    Serial.print("Drop Track Cmd: ");
    Serial.println(cmd);

    if (cmd == "top")
      goDropTo(DROP_TOP);
    else if (cmd == "bottom")
      goDropTo(DROP_BOTTOM);
    else
      stopDrop();
  }
}

// =====================================================
// WIFI
// =====================================================

void connectWifi()
{
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void reconnectMqtt()
{
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ride_controller"))
    {
      Serial.println("connected");
      mqttClient.subscribe("ride/actuator/switchTrack/command");
      mqttClient.subscribe("ride/actuator/dropTrack/command");
      mqttClient.subscribe("ride/actuator/rotateTrack/command");
      mqttClient.subscribe("ride/system/estop");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

// =====================================================
// SENSOR CHECK
// =====================================================

void checkSensors()
{
  bool station = digitalRead(SENSOR_STATION);
  bool sw1     = digitalRead(SENSOR_SWITCH1);
  bool sw2     = digitalRead(SENSOR_SWITCH2);
  bool rot1    = digitalRead(SENSOR_ROTATE1);
  bool top     = digitalRead(SENSOR_DROP2_TOP);
  bool bot     = digitalRead(SENSOR_DROP1_BOTTOM);

  if (station == LOW && lastStation == HIGH) {
    Serial.println("Sensor: Station1 -> 1");
    mqttClient.publish("ride/sensor/Station1/state", "{\"state\":1}");
  }
  if (station == HIGH && lastStation == LOW) {
    Serial.println("Sensor: Station1 -> 0");
    mqttClient.publish("ride/sensor/Station1/state", "{\"state\":0}");
  }

  if (sw1 == LOW && lastSwitch1 == HIGH) {
    Serial.println("Sensor: Switch1 -> 1");
    mqttClient.publish("ride/sensor/Switch1/state", "{\"state\":1}");
  }
  if (sw1 == HIGH && lastSwitch1 == LOW) {
    Serial.println("Sensor: Switch1 -> 0");
    mqttClient.publish("ride/sensor/Switch1/state", "{\"state\":0}");
  }

  if (sw2 == LOW && lastSwitch2 == HIGH) {
    Serial.println("Sensor: Switch2 -> 1");
    mqttClient.publish("ride/sensor/Switch2/state", "{\"state\":1}");
  }
  if (sw2 == HIGH && lastSwitch2 == LOW) {
    Serial.println("Sensor: Switch2 -> 0");
    mqttClient.publish("ride/sensor/Switch2/state", "{\"state\":0}");
  }

  if (rot1 == LOW && lastRotate1 == HIGH) {
    Serial.println("Sensor: Rotate1 -> 1");
    mqttClient.publish("ride/sensor/Rotate1/state", "{\"state\":1}");
  }
  if (rot1 == HIGH && lastRotate1 == LOW) {
    Serial.println("Sensor: Rotate1 -> 0");
    mqttClient.publish("ride/sensor/Rotate1/state", "{\"state\":0}");
  }

  if (bot == LOW && lastDropBot == HIGH) {
    Serial.println("Sensor: Drop1_Bottom -> 1");
    mqttClient.publish("ride/sensor/Drop1/state", "{\"state\":1}");
  }
  if (bot == HIGH && lastDropBot == LOW) {
    Serial.println("Sensor: Drop1_Bottom -> 0");
    mqttClient.publish("ride/sensor/Drop1/state", "{\"state\":0}");
  }

  if (top == LOW && lastDropTop == HIGH) {
    Serial.println("Sensor: Drop2_Top -> 1");
    mqttClient.publish("ride/sensor/Drop2/state", "{\"state\":1}");
  }
  if (top == HIGH && lastDropTop == LOW) {
    Serial.println("Sensor: Drop2_Top -> 0");
    mqttClient.publish("ride/sensor/Drop2/state", "{\"state\":0}");
  }

  lastStation = station;
  lastSwitch1 = sw1;
  lastSwitch2 = sw2;
  lastRotate1 = rot1;
  lastDropTop = top;
  lastDropBot = bot;
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Wire.begin();

  pwm.begin();
  pwm.setPWMFreq(50);   // servos need 50Hz

  stopDrop();

  setServoChannel(SWITCH_SERVO, 0);
  switchCurrent = 0;
  switchTarget  = 0;

  setServoChannel(TURN_SERVO, 7);
  turnCurrent = 7;
  turnTarget  = 7;

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  lastEncoded =
    (digitalRead(ENC_A) << 1) |
     digitalRead(ENC_B);

  attachInterrupt(digitalPinToInterrupt(ENC_A), isrDrop, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), isrDrop, CHANGE);

  pinMode(SENSOR_STATION, INPUT_PULLUP);
  pinMode(SENSOR_SWITCH1, INPUT_PULLUP);
  pinMode(SENSOR_SWITCH2, INPUT_PULLUP);
  pinMode(SENSOR_ROTATE1, INPUT_PULLUP);
  pinMode(SENSOR_DROP2_TOP, INPUT_PULLUP);
  pinMode(SENSOR_DROP1_BOTTOM, INPUT_PULLUP);

  dropCount = DROP_TOP;

  connectWifi();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  Serial.println("SYSTEM READY");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  if (!mqttClient.connected())
    reconnectMqtt();

  mqttClient.loop();

  checkSensors();

  // smooth switch servo
  if (switchCurrent < switchTarget)
  {
    switchCurrent++;
    setServoChannel(SWITCH_SERVO, switchCurrent);
    delay(10);
  }
  else if (switchCurrent > switchTarget)
  {
    switchCurrent--;
    setServoChannel(SWITCH_SERVO, switchCurrent);
    delay(10);
  }

  // smooth turntable servo
  if (turnCurrent < turnTarget)
  {
    turnCurrent++;
    Serial.print("Turntable Moving UP: ");
    Serial.println(turnCurrent);
    setServoChannel(TURN_SERVO, turnCurrent);
    delay(5);
  }
  else if (turnCurrent > turnTarget)
  {
    turnCurrent--;
    Serial.print("Turntable Moving DOWN: ");
    Serial.println(turnCurrent);
    setServoChannel(TURN_SERVO, turnCurrent);
    delay(5);
  }
}