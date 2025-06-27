#include <TMCStepper.h>

// Pin definitions
// EN DIR STEP: 2 4 3 | 9 5 6 | 
#define EN1 30  // Enable (LOW = enabled)
#define DIR1 32  // Direction pin (unused with UART “shaft”)
#define STEP1 31  // Step pulse

// UART interface to the TMC2209
#define SERIAL_PORT  Serial7
#define DRIVER_ADDRESS 0b00

// Motor/driver parameters
#define R_SENSE        0.11f   // Sense resistor value
#define STEPS_PER_REV  200     // Full steps per revolution
#define MICROSTEPS     16      // Microsteps per full step

TMC2209Stepper driver(&SERIAL_PORT, R_SENSE, DRIVER_ADDRESS);

// Motion state
bool rotating = false;
unsigned long totalSteps = 0;
volatile unsigned long stepIndex = 0;
float stepIntervalUs = 0;
bool stepHigh = false;
unsigned long lastTimeUs = 0;

// Rotation sequence (example)
float sequence[][3] = {
  {  0.0f, 180.0f, 200 },  // startAngle, stopAngle, duration in ms
  {180.0f,  45.0f, 100 }
};
const uint8_t seqLen = sizeof(sequence) / sizeof(sequence[0]);
uint8_t seqIndex = 0;

void setup() {
  pinMode(13, OUTPUT); 
  digitalWrite(13, HIGH); 

  pinMode(EN1,   OUTPUT);
  pinMode(STEP1, OUTPUT);
  pinMode(DIR1,  OUTPUT);

  digitalWrite(EN1, LOW);      // Enable driver hardware

  SERIAL_PORT.begin(115200);
  driver.begin();                 // Init UART/SPI
  driver.toff(5);
  driver.rms_current(600);        // 600 mA RMS
  driver.microsteps(MICROSTEPS);
  driver.pwm_autoscale(true);

  // Kick off first rotation
  startRotation(sequence[seqIndex][0], sequence[seqIndex][1], (unsigned long)sequence[seqIndex][2]);
}

void loop() {
  updateRotation();

  // Once current rotation finishes, advance to next in sequence
  if (!rotating) {
    seqIndex = (seqIndex + 1) % seqLen;
    delay(1000);  // small pause between moves, not part of step timing
    startRotation(sequence[seqIndex][0], sequence[seqIndex][1], (unsigned long)sequence[seqIndex][2]);
  }
}

/**
 * Initialize a non-blocking rotation.
 * @param startAngle  in degrees [0..360)
 * @param stopAngle   in degrees [0..360)
 * @param durationMs  time to span the movement
 */
void startRotation(float startAngle, float stopAngle, unsigned long durationMs) {
  // Wrap angles
  auto wrapDeg = [](float a) {
    while (a < 0.0f) a += 360.0f;
    while (a >= 360.0f) a -= 360.0f;
    return a;
  };
  startAngle = wrapDeg(startAngle);
  stopAngle  = wrapDeg(stopAngle);

  // Compute shortest delta
  float delta = stopAngle - startAngle;
  if (delta > 180.0f) delta -= 360.0f;
  if (delta < -180.0f) delta += 360.0f;

  bool dir = (delta >= 0);
  driver.shaft(dir);

  float stepsPerRev = STEPS_PER_REV * MICROSTEPS;
  totalSteps = (unsigned long)(fabs(delta) / 360.0f * stepsPerRev + 0.5f);
  if (totalSteps == 0) return;

  // microseconds between step edges
  stepIntervalUs = (durationMs * 1000.0f) / totalSteps;

  // reset motion state
  stepIndex    = 0;
  stepHigh     = false;
  lastTimeUs   = micros();
  rotating     = true;

  digitalWrite(STEP1, LOW);
}

/**
 * Called frequently in loop() to progress the motion without blocking.
 */
void updateRotation() {
  if (!rotating) return;

  unsigned long nowUs = micros();
  unsigned long elapsed = nowUs - lastTimeUs;

  if (!stepHigh) {
    // time for next rising edge?
    if (elapsed >= (unsigned long)stepIntervalUs) {
      digitalWrite(STEP1, HIGH);
      lastTimeUs = nowUs;
      stepHigh = true;
    }
  } else {
    // finish the pulse (min 2 µs)
    if (elapsed >= 2) {
      digitalWrite(STEP1, LOW);
      lastTimeUs = nowUs;
      stepHigh = false;
      if (++stepIndex >= totalSteps) {
        rotating = false;
      }
    }
  }
}
