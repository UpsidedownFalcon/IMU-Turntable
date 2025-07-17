#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TMCStepper.h>

// Pin definitions for three drivers
#define EN1   2
#define STEP1 3

#define EN2   9
#define STEP2 6

#define EN3   30
#define STEP3 31

// UART interfaces
#define SERIAL1_PORT Serial1
#define SERIAL2_PORT Serial2
#define SERIAL3_PORT Serial7
#define DRIVER_ADDRESS 0b00

// Motor parameters
#define R_SENSE       0.11f
#define STEPS_PER_REV 200
#define MICROSTEPS    16
const float STEP_ANGLE   = 360.0f / STEPS_PER_REV;        // 1.8°
const float THETA_RES    = STEP_ANGLE / MICROSTEPS;       // 0.1125° per microstep

// SD card
#define SD_CS BUILTIN_SDCARD
const char* TRAJ_DIR = "/traj";

// Driver objects
TMC2209Stepper driver1(&SERIAL1_PORT, R_SENSE, DRIVER_ADDRESS);
TMC2209Stepper driver2(&SERIAL2_PORT, R_SENSE, DRIVER_ADDRESS);
TMC2209Stepper driver3(&SERIAL3_PORT, R_SENSE, DRIVER_ADDRESS);

// Trajectory state per axis
struct TrajState {
  File      file;
  uint32_t  totalSamples;
  uint32_t  idx;            // next sample index (1..N-1)
  float     prevAngle;
  float     nextTime;       // seconds
  float     nextAngle;
  bool      done;
  int       stepPin;
  TMC2209Stepper* driver;
} traj[3];

// Start time reference
uint32_t startMicros;

// Helper to read a little-endian float from SD
float readFloatLE(File& f) {
  float v;
  f.read(&v, sizeof(v));
  return v;
}

void setup() {
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  // Enable pins
  pinMode(EN1, OUTPUT); pinMode(EN2, OUTPUT); pinMode(EN3, OUTPUT);
  digitalWrite(EN1, LOW); digitalWrite(EN2, LOW); digitalWrite(EN3, LOW);

  // Step pins low
  pinMode(STEP1, OUTPUT); pinMode(STEP2, OUTPUT); pinMode(STEP3, OUTPUT);
  digitalWrite(STEP1, LOW); digitalWrite(STEP2, LOW); digitalWrite(STEP3, LOW);

  // Init UART drivers
  SERIAL1_PORT.begin(115200);
  driver1.begin(); driver1.toff(5); driver1.rms_current(2000);
  driver1.microsteps(MICROSTEPS); driver1.pwm_autoscale(true);

  SERIAL2_PORT.begin(115200);
  driver2.begin(); driver2.toff(5); driver2.rms_current(2000);
  driver2.microsteps(MICROSTEPS); driver2.pwm_autoscale(true);

  SERIAL3_PORT.begin(115200);
  driver3.begin(); driver3.toff(5); driver3.rms_current(2000);
  driver3.microsteps(MICROSTEPS); driver3.pwm_autoscale(true);

  // Mount SD
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed");
    while (true) {}
  }
  // ensure folder
  if (!SD.exists(TRAJ_DIR)) {
    SD.mkdir(TRAJ_DIR);
  }

  // Open & prime trajectories
  const char* names[3] = { "X", "Y", "Z" };
  for (int i = 0; i < 3; i++) {
    traj[i].stepPin   = (i==0? STEP1 : (i==1? STEP2 : STEP3));
    traj[i].driver    = (i==0? &driver1 : (i==1? &driver2 : &driver3));
    traj[i].done      = false;
    String path = String(TRAJ_DIR) + "/" + names[i] + ".bin";
    traj[i].file = SD.open(path.c_str(), FILE_READ);
    if (!traj[i].file) {
      Serial.print("ERR: cannot open ");
      Serial.println(path);
      traj[i].done = true;
      continue;
    }
    // read sample count
    uint32_t N = 0;
    traj[i].file.read(&N, sizeof(N));
    traj[i].totalSamples = N;
    if (N < 2) {
      traj[i].done = true;
      continue;
    }
    // read first sample (t0, a0)
    float t0 = readFloatLE(traj[i].file);
    float a0 = readFloatLE(traj[i].file);
    traj[i].prevAngle = a0;
    // read second sample (t1, a1)
    traj[i].nextTime  = readFloatLE(traj[i].file);
    traj[i].nextAngle = readFloatLE(traj[i].file);
    traj[i].idx       = 1;
  }

  // record start timestamp
  startMicros = micros();
}

void loop() {
  uint32_t nowUs = micros();
  float    elapsedSec = (nowUs - startMicros) * 1e-6f;

  bool allDone = true;
  for (int i = 0; i < 3; i++) {
    if (traj[i].done) continue;
    allDone = false;

    // Time to apply this sample?
    if (elapsedSec >= traj[i].nextTime) {
      // compute angle delta
      float diff = traj[i].nextAngle - traj[i].prevAngle;
      if (fabs(diff) >= (THETA_RES * 0.5f)) {
        // set direction via UART driver
        bool dir = (diff >= 0);
        traj[i].driver->shaft(dir);

        // emit one microstep pulse
        digitalWrite(traj[i].stepPin, HIGH);
        delayMicroseconds(2);
        digitalWrite(traj[i].stepPin, LOW);
      }
      // advance
      traj[i].prevAngle = traj[i].nextAngle;
      traj[i].idx++;
      if (traj[i].idx < traj[i].totalSamples) {
        // read next (t,a)
        traj[i].nextTime  = readFloatLE(traj[i].file);
        traj[i].nextAngle = readFloatLE(traj[i].file);
      } else {
        // done with this axis
        traj[i].file.close();
        traj[i].done = true;
      }
    }
  }

  // optionally blink LED when all done
  if (allDone) {
    digitalWrite(13, !digitalRead(13));
    delay(200);
  }
}
