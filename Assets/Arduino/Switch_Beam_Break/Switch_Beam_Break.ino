/*
  Arduino UNO R4 + PCA9685
  1 Servo sweeps back and forth
  3 DC Motors (forward / reverse)

  No WiFi required

  Library:
  Adafruit PWM Servo Driver
*/

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// ---------------- PCA9685 CHANNELS ----------------
// Servo
const int SERVO_CH = 0;

// Motor 1
const int M1_FWD = 1;
const int M1_REV = 2;

// Motor 2
const int M2_FWD = 3;
const int M2_REV = 4;

// Motor 3
const int M3_FWD = 5;
const int M3_REV = 6;

// ---------------- SERVO SETTINGS ----------------
const int SERVO_MIN = 120;
const int SERVO_MAX = 620;

int angle = 0;
int direction = 1;

// ---------------- FUNCTIONS ----------------
int angleToPulse(int angle) {
  return map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
}

void setPWM(int channel, int value) {
  value = constrain(value, 0, 4095);
  pwm.setPWM(channel, 0, value);
}

// speed = -4095 to +4095
void runMotor(int chFwd, int chRev, int speedValue) {
  speedValue = constrain(speedValue, -4095, 4095);

  if (speedValue > 0) {
    setPWM(chFwd, speedValue);
    setPWM(chRev, 0);
  }
  else if (speedValue < 0) {
    setPWM(chFwd, 0);
    setPWM(chRev, -speedValue);
  }
  else {
    setPWM(chFwd, 0);
    setPWM(chRev, 0);
  }
}

void stopAllMotors() {
  runMotor(M1_FWD, M1_REV, 0);
  runMotor(M2_FWD, M2_REV, 0);
  runMotor(M3_FWD, M3_REV, 0);
}

// ---------------- SETUP ----------------
void setup() {
  Wire.begin();

  pwm.begin();
  pwm.setPWMFreq(50);   // works for servo + motors

  stopAllMotors();
}

// ---------------- LOOP ----------------
void loop() {

  // ---------- Servo Sweep ----------
  pwm.setPWM(SERVO_CH, 0, angleToPulse(angle));

  angle += direction;

  if (angle >= 180) {
    angle = 180;
    direction = -1;
  }

  if (angle <= 0) {
    angle = 0;
    direction = 1;
  }

  // ---------- Motors Example ----------
  // Motor 1 forward
  runMotor(M1_FWD, M1_REV, 3000);

  // // Motor 2 reverse
  runMotor(M2_FWD, M2_REV, -2500);

  // // Motor 3 stopped
  // runMotor(M3_FWD, M3_REV, 0);

  delay(15);
}