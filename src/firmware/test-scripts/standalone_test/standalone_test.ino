// Pin definitions
const uint8_t DIR_PIN    = 37;
const uint8_t STEP_PIN   = 36; 
const uint8_t ENABLE_PIN = 33; 

const uint16_t STEPS_PER_REV     = 2000;       // 1.8° motor → 200 full-steps/rev
const unsigned long STEP_INTERVAL = 2500UL;  // µs between step *starts* (3 000 000 µs / 200)
const unsigned int PULSE_WIDTH    = 10;       // keep STEP high at least 10 µs (>100 ns requirement)

// State machine for stepping
enum StepState { IDLE, PULSE_HIGH };
volatile StepState stepState = IDLE;

// Bookkeeping
unsigned long lastEventTime = 0;   // timestamp of last edge (rising or falling)
uint16_t stepsTaken        = 0;    // how many steps we've done

void setup() {
  // (Optional) onboard LED to show “we’re alive”
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  pinMode(DIR_PIN,    OUTPUT);
  pinMode(STEP_PIN,   OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);

  digitalWrite(ENABLE_PIN, LOW);   // enable driver (active LOW)
  digitalWrite(DIR_PIN,    HIGH);  // clockwise

  // initialize timers/state
  lastEventTime = micros();
  stepState     = IDLE;
}

void loop() {
  unsigned long now = micros();

  // Still have steps to perform?
  if (stepsTaken < STEPS_PER_REV) {
    if (stepState == IDLE) {
      // Time to start a new pulse?
      if (now - lastEventTime >= STEP_INTERVAL) {
        digitalWrite(STEP_PIN, HIGH);
        lastEventTime = now;
        stepState     = PULSE_HIGH;
      }
    }
    else { // PULSE_HIGH
      // Time to end the pulse?
      if (now - lastEventTime >= PULSE_WIDTH) {
        digitalWrite(STEP_PIN, LOW);
        lastEventTime = now;
        stepState     = IDLE;
        ++stepsTaken;
      }
    }
  }
  else {
    // Finished one full rev: disable driver and blink LED on pin 13
    digitalWrite(ENABLE_PIN, HIGH);
    // (Optional) flash LED to signal done
    if (now - lastEventTime >= 500000UL) {        // every 0.5 s
      digitalWrite(13, !digitalRead(13));
      lastEventTime = now;
    }
    // You could also stop calling loop entirely by sleeping, etc.
  }

  // --- here you can do other non-blocking work! ---
}
