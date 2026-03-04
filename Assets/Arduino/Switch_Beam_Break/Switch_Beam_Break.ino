const int redPin = 5;
const int greenPin = 6;
const int bluePin = 9;

void setup(){
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  
  Serial.begin(9600);
}
void loop() {
    digitalWrite(redPin, HIGH);
    delay(200);
    digitalWrite(redPin, LOW);
    delay(200);
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, LOW);
    digitalWrite(bluePin, LOW);
}