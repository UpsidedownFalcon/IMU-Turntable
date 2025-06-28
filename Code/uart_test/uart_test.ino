#include <TMCStepper.h>

// Pin definitions for three drivers
#define EN1   2
#define DIR1  4
#define STEP1 3

#define EN2   9
#define DIR2  5
#define STEP2 6

#define EN3   30
#define DIR3  32
#define STEP3 31

// UART interfaces
#define SERIAL1_PORT Serial1
#define SERIAL2_PORT Serial2
#define SERIAL3_PORT Serial7
#define DRIVER_ADDRESS 0b00

// Motor parameters
#define R_SENSE       0.20f
#define STEPS_PER_REV 200
#define MICROSTEPS    16

// Driver objects
TMC2209Stepper driver1(&SERIAL1_PORT, R_SENSE, DRIVER_ADDRESS);
TMC2209Stepper driver2(&SERIAL2_PORT, R_SENSE, DRIVER_ADDRESS);
TMC2209Stepper driver3(&SERIAL3_PORT, R_SENSE, DRIVER_ADDRESS);

// Per-motor motion state
struct Motion {
  bool rotating;
  unsigned long totalSteps;
  unsigned long stepIndex;
  float stepIntervalUs;
  bool stepHigh;
  unsigned long lastTimeUs;
};
Motion motors[3];



void setup() {
  // ––––– Serial for debugging –––––
  Serial.begin(115200);
  while (!Serial);               // wait for host

  // ––––– Alive LED –––––
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  // ––––– Driver enable pins (active LOW) –––––
  pinMode(EN1, OUTPUT); pinMode(EN2, OUTPUT); pinMode(EN3, OUTPUT);
  digitalWrite(EN1, LOW);    digitalWrite(EN2, LOW);    digitalWrite(EN3, LOW);

  // ––––– STEP pins start LOW –––––
  pinMode(STEP1, OUTPUT); pinMode(STEP2, OUTPUT); pinMode(STEP3, OUTPUT);
  digitalWrite(STEP1, LOW);   digitalWrite(STEP2, LOW);   digitalWrite(STEP3, LOW);

  // ––––– Initialize & configure each TMC2209 –––––
  SERIAL1_PORT.begin(115200);
  driver1.begin();
  driver1.toff(5);
  driver1.rms_current(2000);
  driver1.microsteps(MICROSTEPS);
  driver1.pwm_autoscale(true);

  SERIAL2_PORT.begin(115200);
  driver2.begin();
  driver2.toff(5);
  driver2.rms_current(2000);
  driver2.microsteps(MICROSTEPS);
  driver2.pwm_autoscale(true);

  SERIAL3_PORT.begin(115200);
  driver3.begin();
  driver3.toff(5);
  driver3.rms_current(2000);
  driver3.microsteps(MICROSTEPS);
  driver3.pwm_autoscale(true);

  // ––––– UART link & register‐read checks –––––
  Serial.println("===== UART & Register Check =====");

  // Driver 1
  {
    bool ok = driver1.test_connection();
    Serial.print("Driver1 UART: ");
    Serial.println(ok ? "OK" : "FAIL");
    Serial.print("  GCONF = 0x");
    Serial.println(driver1.GCONF(), HEX);
    Serial.print("  IHOLD_IRUN = 0x");
    Serial.println(driver1.IHOLD_IRUN(), HEX);
  }

  // Driver 2
  {
    bool ok = driver2.test_connection();
    Serial.print("Driver2 UART: ");
    Serial.println(ok ? "OK" : "FAIL");
    Serial.print("  GCONF = 0x");
    Serial.println(driver2.GCONF(), HEX);
    Serial.print("  IHOLD_IRUN = 0x");
    Serial.println(driver2.IHOLD_IRUN(), HEX);
  }

  // Driver 3
  {
    bool ok = driver3.test_connection();
    Serial.print("Driver3 UART: ");
    Serial.println(ok ? "OK" : "FAIL");
    Serial.print("  GCONF = 0x");
    Serial.println(driver3.GCONF(), HEX);
    Serial.print("  IHOLD_IRUN = 0x");
    Serial.println(driver3.IHOLD_IRUN(), HEX);
  }

  Serial.println("=================================");

  delay(100);  // give the prints time to flush

  // ––––– Start your move –––––
  startRotation(0.0f, 360.0f, 3000UL);
}




void loop() {
  // Progress motor steps until done
  updateRotation();
  digitalWrite(13, LOW); 
  // delay(5000); 

  // while(true){
  //   digitalWrite(EN1, HIGH); 
  //   digitalWrite(EN2, HIGH);
  //   digitalWrite(EN3, HIGH);  

  //   digitalWrite(13, !digitalRead(13)); 
  //   delay(1000); 
  // }
}

/**
 * Start a simultaneous rotation on all three motors.
 * @param startAngle  Start angle in degrees
 * @param stopAngle   Stop angle in degrees
 * @param durationMs  Duration of motion in milliseconds
 */
void startRotation(float startAngle, float stopAngle, unsigned long durationMs) {
  auto wrapDeg = [](float a) {
    while (a < 0) a += 360;
    while (a >= 360) a -= 360;
    return a;
  };
  startAngle = wrapDeg(startAngle);
  stopAngle  = wrapDeg(stopAngle);

  // Compute signed delta, treat full rotation specially
  float delta = stopAngle - startAngle;
  if (fabs(delta) < 1e-3f) {
    // if start and stop equal, do full rotation
    delta = 360.0f;
  } else {
    // shortest path for partial moves
    if (delta > 180.0f)  delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
  }

  bool dir = (delta >= 0);
  // Set each driver direction via UART
  driver1.shaft(dir);
  driver2.shaft(dir);
  driver3.shaft(dir);

  unsigned long steps = (unsigned long)(fabs(delta) / 360.0f * (STEPS_PER_REV * MICROSTEPS) + 0.5f);
  if (steps == 0) return;
  float interval = (durationMs * 1000.0f) / steps;

  // Initialize each motor's state
  for (int i = 0; i < 3; ++i) {
    motors[i].rotating     = true;
    motors[i].totalSteps   = steps;
    motors[i].stepIndex    = 0;
    motors[i].stepIntervalUs = interval;
    motors[i].stepHigh     = false;
    motors[i].lastTimeUs   = micros();
    // Ensure step line low
    digitalWrite((i==0 ? STEP1 : (i==1 ? STEP2 : STEP3)), LOW);
  }
}

/**
 * Non-blocking update: call frequently to progress all motors.
 */
void updateRotation() {
  unsigned long now = micros();
  for (int i = 0; i < 3; ++i) {
    Motion &m = motors[i];
    if (!m.rotating) continue;

    unsigned long elapsed = now - m.lastTimeUs;
    int stepPin = (i==0 ? STEP1 : (i==1 ? STEP2 : STEP3));

    if (!m.stepHigh) {
      if (elapsed >= (unsigned long)m.stepIntervalUs) {
        digitalWrite(stepPin, HIGH);
        m.lastTimeUs = now;
        m.stepHigh   = true;
      }
    } else {
      if (elapsed >= 2) {
        digitalWrite(stepPin, LOW);
        m.lastTimeUs = now;
        m.stepHigh   = false;
        if (++m.stepIndex >= m.totalSteps) {
          m.rotating = false;
        }
      }
    }
  }
}
