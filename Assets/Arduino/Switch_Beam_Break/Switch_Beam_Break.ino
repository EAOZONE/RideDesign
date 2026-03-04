#include <WiFiS3.h>
#include <PubSubClient.h>
#include <Servo.h>

// --------------------
// WiFi Settings
// --------------------
const char* ssid = "Ben";
const char* password = "vszu6851";

// --------------------
// MQTT Settings
// --------------------
const char* mqtt_server = "10.160.121.73";
const int mqtt_port = 1883;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// --------------------
// Beam Settings
// --------------------
const int beamStartPin = 2;   // START trigger
const int beamEndPin   = 3;   // END trigger

bool lastStartState = HIGH;
bool lastEndState   = HIGH;

// --------------------
// Servo Settings
// --------------------
Servo testServo;
const int servoPin = 5;

// --------------------
// MQTT Callback
// --------------------
void callback(char* topic, byte* payload, unsigned int length)
{
  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println("Received: " + message);

  if (message.indexOf("\"id\":\"SwitchBeam\"") != -1)
  {
    if (message.indexOf("\"state\":1") != -1)
    {
      Serial.println("SwitchTrack → 90°");
      testServo.write(90);
    }

    if (message.indexOf("\"state\":0") != -1)
    {
      Serial.println("SwitchTrack → 0°");
      testServo.write(0);
    }
  }
}

// --------------------
// Connect WiFi
// --------------------
void connectWiFi() {

  IPAddress local_IP(10,160,121,200);
  IPAddress gateway(10,160,121,167);
  IPAddress subnet(255,255,255,0);

  WiFi.config(local_IP, gateway, subnet);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi FAILED");
  }
}

// --------------------
// Connect MQTT
// --------------------
void connectMQTT() {

  while (!client.connected()) {

    Serial.println("Connecting to MQTT...");

    String clientId = "WaysideNode-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {

      Serial.println("MQTT connected");

      client.subscribe("wayside/beam");
      Serial.println("Subscribed to wayside/beam");

    } else {

      Serial.print("MQTT failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 2 seconds...");

      delay(2000);
    }
  }
}

// --------------------
// Setup
// --------------------
void setup() {

  Serial.begin(115200);
  delay(2000);

  pinMode(beamStartPin, INPUT_PULLUP);
  pinMode(beamEndPin, INPUT_PULLUP);

  testServo.attach(servoPin);
  testServo.write(0);

  connectWiFi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(512);
  client.setKeepAlive(30);
  client.setSocketTimeout(5);
}

// --------------------
// Main Loop
// --------------------
void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();

  bool currentStart = digitalRead(beamStartPin);
  bool currentEnd   = digitalRead(beamEndPin);

  // --------------------
  // START beam triggered
  // --------------------
  if (lastStartState == HIGH && currentStart == LOW) {

    Serial.println("SwitchBeam START");
    delay(300);
    testServo.write(90);

    client.publish(
      "wayside/beam",
      "{\"id\":\"SwitchBeam\",\"state\":1}"
    );

    delay(100);
  }

  // --------------------
  // END beam triggered
  // --------------------
  if (lastEndState == HIGH && currentEnd == LOW) {

    Serial.println("SwitchBeam END");
    delay(300);
    testServo.write(0);

    client.publish(
      "wayside/beam",
      "{\"id\":\"SwitchBeam\",\"state\":0}"
    );

    delay(100);
  }

  lastStartState = currentStart;
  lastEndState   = currentEnd;
}