#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char* ssid = "Ben";
const char* password = "vszu6851";

const char* mqtt_server = "10.51.222.143";

WiFiClient espClient;
PubSubClient client(espClient);

const int AIA = 3;
const int AIB = 4;
const int BIA = 9;
const int BIB = 10;
const int irSwitchPin = 13;
int lastIrState = HIGH;

int speed = 0;

String vehicle_id = "1";
String topic = "ride/vehicle/1/drive/command";

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  StaticJsonDocument<200> doc;

  deserializeJson(doc, payload, length);

  speed = doc["speed"];

  Serial.print("Received speed: ");
  Serial.println(speed);

  if (speed >= 0){
    forward();
  }
  else
  {
    backward();
  }
}

void reconnect() {

  while (!client.connected()) {

    if (client.connect("vehicle_client")) {

      client.subscribe("ride/vehicle/0/drive/command");

    } else {

      delay(2000);
    }
  }
}

void setup() {

  Serial.begin(115200);

  pinMode(AIA, OUTPUT);
  pinMode(AIB, OUTPUT);
  pinMode(BIA, OUTPUT);
  pinMode(BIB, OUTPUT);
  pinMode(irSwitchPin, INPUT);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  int irState = digitalRead(irSwitchPin);
  Serial.println(irState);
  if (irState == LOW && lastIrState == HIGH) {
    publishSwitchTrack();
  }

  lastIrState = irState;
}
void publishSwitchTrack() {

  const char* topic = "ride/sensor/Switch1/state";

  StaticJsonDocument<100> doc;
  doc["Sensor"] = "Switch1";
  doc["state"] = 1;

  char buffer[128];
  serializeJson(doc, buffer);

  client.publish(topic, buffer);

  Serial.println("Published switchTrack trigger");
}
void forward()
{
  analogWrite(AIA, speed);
  analogWrite(AIB, 0);
  analogWrite(BIA, speed);
  analogWrite(BIB, 0);
}

void backward()
{
  analogWrite(AIA, 0);
  analogWrite(AIB, -speed);
  analogWrite(BIA, 0);
  analogWrite(BIB, -speed);
}