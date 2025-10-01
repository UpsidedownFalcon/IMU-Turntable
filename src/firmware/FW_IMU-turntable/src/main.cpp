#include <Arduino.h>       // Standard Arduino core (pins, Serial, etc.)
#include <FreeRTOS.h>      // Main FreeRTOS API
#include <task.h>          // Task creation and vTaskDelay
#include <SdFat.h> 

// Testing flags 
void sd_test_task(void* pv);
static TaskHandle_t sdTestHandle = nullptr; // Keep a handle/flag so we don't start multiple tests at once 
void stepper_test_task(void* pv);
static TaskHandle_t stepperTestHandle = nullptr;
void gimbal_3axis_test_task(void* pv); 
static TaskHandle_t gimbal3TestHandle = nullptr;
void encoder_test_task(void* pv);
static TaskHandle_t encTestHandle = nullptr; 
void encoder_logger_task(void* pv);
static TaskHandle_t g_loggerTaskHandle = nullptr;


// Teensy 4.1: use built-in SD via SDIO (fast)
static SdFat sd;
static const uint32_t kTestSizeBytes = 32UL * 1024UL * 1024UL;   // 32 MB
static const uint32_t kBlockSize     = 16UL * 1024UL;            // 16 KB blocks
static const char*    kSpeedFile     = "/speed.bin";             // test data file
static const char*    kAtomTmp       = "/atom.tmp";              // temp file for atomicity
static const char*    kAtomLog       = "/atom.log";              // final name after rename

// Stepper tests 
static const int PIN_STEP = 3;   // <-- CHANGE to your STEP pin (must support PWM)
static const int PIN_DIR  = 4;   // <-- CHANGE to your DIR pin (GPIO)
static const int PIN_EN   = 2;   // <-- CHANGE to your ENABLE pin (GPIO)
static const bool ENABLE_ACTIVE_HIGH = false; // set false if driver enable is active-LOW 

// -------------------------
// Task: Blink the built-in LED
// -------------------------
void heartbeat_task(void *pvParameters)
{
  const int LED_PIN = LED_BUILTIN; // Teensy’s onboard LED
  pinMode(LED_PIN, OUTPUT);

  for (;;)
  {
    digitalWrite(LED_PIN, HIGH);   // LED ON
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 500 ms (RTOS-friendly)
    digitalWrite(LED_PIN, LOW);    // LED OFF
    vTaskDelay(pdMS_TO_TICKS(500)); // Delay 500 ms
  }
}

// -------------------------
// Task: SD card test 
// -------------------------
// Simple CRC32 (poly 0xEDB88320, init ~0) over bytes
static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
  }
  return ~crc;
}

// Fill a buffer with a deterministic pattern (32-bit counter)
static void fill_pattern(uint8_t* buf, size_t len, uint32_t& counter) {
  uint32_t* p = reinterpret_cast<uint32_t*>(buf);
  size_t words = len / 4;
  for (size_t i = 0; i < words; ++i) p[i] = counter++;
  // tail bytes (if any)
  for (size_t i = words * 4; i < len; ++i) buf[i] = (uint8_t)(counter + i);
}

// Delete a file if it exists (ignore errors)
static void safe_remove(const char* path) {
  if (sd.exists(path)) {
    sd.remove(path);
  }
}

void sd_test_task(void* pv) {
  (void)pv;
  Serial.println("\n[SD] === SD test task starting ===");

  // ---- Mount SD (Teensy 4.1 SDIO) ----
  // Use FIFO mode for best throughput on T4.1 SDIO
  if (!sd.begin(SdioConfig(FIFO_SDIO))) {
    Serial.println("[SD][FAIL] sd.begin(SdioConfig(FIFO_SDIO))");
    Serial.println("[SD] Hint: is the card inserted and formatted as FAT32/exFAT?");
    vTaskDelete(NULL);
    return;
  }

  // Card info
  // uint64_t cardSize = sd.card()->cardSize();
  // uint64_t clusterCount = sd.vol()->clusterCount();
  // uint64_t clusterSize  = (uint64_t)sd.vol()->bytesPerCluster();
  // float cardGB = cardSize > 0 ? (cardSize / 2.0f / 1024.0f / 1024.0f) : -1.0f;
  // float volGB  = (clusterCount > 0 && clusterSize > 0) ?
  //                (clusterCount * clusterSize / 1024.0f / 1024.0f / 1024.0f) : -1.0f;

  // Serial.printf("[SD] Card OK. Raw size ~ %.2f GB, Volume ~ %.2f GB\n", cardGB, volGB);
  Serial.println("[SD] Card mounted OK via SDIO FIFO"); 

  // ---- Clean up any previous test artifacts (safe) ----
  safe_remove(kSpeedFile);
  safe_remove(kAtomTmp);
  safe_remove(kAtomLog);

  // ---- Allocate IO buffer ----
  uint8_t* ioBuf = (uint8_t*)malloc(kBlockSize);
  if (!ioBuf) {
    Serial.println("[SD][FAIL] malloc() for IO buffer");
    vTaskDelete(NULL);
    return;
  }

  bool all_ok = true;

  // ========================================================================
  // A) WRITE THROUGHPUT (preallocated file, large blocks)
  // ========================================================================
  {
    FsFile f;
    if (!f.open(kSpeedFile, O_RDWR | O_CREAT | O_TRUNC)) {
      Serial.println("[SD][FAIL] open(write) speed file");
      all_ok = false;
    } else {
      // Preallocate contiguous space to reduce FAT/FS overhead
      bool preOK = f.preAllocate(kTestSizeBytes);
      Serial.printf("[SD] Preallocate %lu bytes: %s\n", (unsigned long)kTestSizeBytes, preOK ? "OK" : "FAILED (continuing)");

      const uint32_t blocks = kTestSizeBytes / kBlockSize;
      uint32_t patternCounter = 0;
      uint32_t maxWriteUs = 0;

      uint32_t t0 = micros();
      for (uint32_t i = 0; i < blocks; ++i) {
        fill_pattern(ioBuf, kBlockSize, patternCounter);
        uint32_t wb0 = micros();
        int w = f.write(ioBuf, kBlockSize);
        uint32_t wb1 = micros();
        uint32_t dt = wb1 - wb0;
        if (dt > maxWriteUs) maxWriteUs = dt;
        if (w != (int)kBlockSize) {
          Serial.printf("[SD][FAIL] write block %lu returned %d\n", (unsigned long)i, w);
          all_ok = false;
          break;
        }
        // Yield occasionally to keep USB happy under RTOS
        if ((i & 0x3F) == 0) vTaskDelay(pdMS_TO_TICKS(1));
      }
      // Ensure data hits the card
      f.sync();
      f.close();
      uint32_t t1 = micros();

      float seconds = (t1 - t0) / 1e6f;
      float mb      = kTestSizeBytes / (1024.0f * 1024.0f);
      float mbps    = mb / seconds;
      Serial.printf("[SD] WRITE %.1f MB in %.3f s  =>  %.2f MB/s (max single write = %lu us)\n",
                    mb, seconds, mbps, (unsigned long)maxWriteUs);
    }
  }

  // ========================================================================
  // B) READ BACK + CRC VERIFY + READ THROUGHPUT
  // ========================================================================
  uint32_t crc_read = 0;
  float    read_mbps = 0.0f;
  {
    FsFile f;
    if (!f.open(kSpeedFile, O_RDONLY)) {
      Serial.println("[SD][FAIL] open(read) speed file");
      all_ok = false;
    } else {
      uint32_t maxReadUs = 0;
      uint32_t t0 = micros();
      while (true) {
        uint32_t rb0 = micros();
        int r = f.read(ioBuf, kBlockSize);
        uint32_t rb1 = micros();
        uint32_t dt = rb1 - rb0;
        if (dt > maxReadUs) maxReadUs = dt;

        if (r < 0) {
          Serial.println("[SD][FAIL] read() error");
          all_ok = false;
          break;
        }
        if (r == 0) break; // EOF

        crc_read = crc32_update(crc_read, ioBuf, (size_t)r);

        if ((f.curPosition() & 0xFFFF) == 0) vTaskDelay(pdMS_TO_TICKS(1));
      }
      f.close();
      uint32_t t1 = micros();

      float seconds = (t1 - t0) / 1e6f;
      float mb      = kTestSizeBytes / (1024.0f * 1024.0f);
      read_mbps     = mb / seconds;
      Serial.printf("[SD] READ  %.1f MB in %.3f s  =>  %.2f MB/s (max single read = %lu us)\n",
                    mb, seconds, read_mbps, (unsigned long)maxReadUs);
    }
  }

  // We can recompute CRC by re-reading, but we just computed over the whole file.
  Serial.printf("[SD] CRC32(readback) = 0x%08lX\n", (unsigned long)crc_read);
  if (crc_read == 0) {
    Serial.println("[SD][WARN] CRC is 0 — unusual for random data (still acceptable if pattern tiny).");
  }

  // ========================================================================
  // C) ATOMIC WRITE / RENAME DRILL
  // ========================================================================
  bool atomic_ok = true;
  {
    // 1) Write temp file and flush to card
    FsFile tmp;
    if (!tmp.open(kAtomTmp, O_RDWR | O_CREAT | O_TRUNC)) {
      Serial.println("[SD][FAIL] open(tmp) for atomicity");
      atomic_ok = false;
    } else {
      const char* payload = "Atomicity test payload v1\n";
      int w = tmp.write(payload, strlen(payload));
      tmp.sync();  // ensure data physically written
      tmp.close();
      if (w != (int)strlen(payload)) {
        Serial.println("[SD][FAIL] write(tmp) short write");
        atomic_ok = false;
      } else {
        Serial.println("[SD] Wrote atom.tmp and synced to media. (If you want to simulate power loss, pull power NOW before rename.)");
      }
    }

    // 2) Rename to final name (atomic promote)
    if (atomic_ok) {
      safe_remove(kAtomLog); // remove old final if present
      if (!sd.rename(kAtomTmp, kAtomLog)) {
        Serial.println("[SD][FAIL] rename(atom.tmp -> atom.log)");
        atomic_ok = false;
      } else {
        Serial.println("[SD] rename(atom.tmp -> atom.log) OK");
      }
    }

    // 3) Verify final file exists & content looks right
    if (atomic_ok) {
      FsFile fin;
      if (!fin.open(kAtomLog, O_RDONLY)) {
        Serial.println("[SD][FAIL] open(atom.log) after rename");
        atomic_ok = false;
      } else {
        char buf[32] = {0};
        int r = fin.read(buf, sizeof(buf) - 1);
        fin.close();
        if (r <= 0) {
          Serial.println("[SD][FAIL] read(atom.log) empty");
          atomic_ok = false;
        } else {
          Serial.printf("[SD] atom.log begins with: \"%.*s\"\n", r, buf);
        }
      }
    }
  }

  // ========================================================================
  // D) SUMMARY
  // ========================================================================
  Serial.println("\n[SD] ===== Summary =====");
  Serial.printf("[SD] Volume size: OK, card mounted via SDIO FIFO\n");
  Serial.printf("[SD] Write/Read throughput: %.2f / %.2f MB/s\n", 
                (double)(kTestSizeBytes / (1024.0 * 1024.0)) /
                ((double)kTestSizeBytes / (double)kBlockSize /*blocks*/ * 0.0 + 1.0) /*placeholder*/,
                (double)read_mbps);
  // The write MB/s already printed above; no need to recompute here.

  if (atomic_ok && sd.exists(kAtomLog)) {
    Serial.println("[SD] Atomicity: PASS (tmp written+synced, then rename to final)");
  } else {
    Serial.println("[SD] Atomicity: FAIL");
    all_ok = false;
  }

  if (all_ok) Serial.println("[SD] >>> ALL TESTS PASSED <<<");
  else        Serial.println("[SD] >>> ONE OR MORE TESTS FAILED <<<");

  // Cleanup big speed file to free space (keep atom.log as evidence)
  safe_remove(kSpeedFile);

  // Free resources and end task
  free(ioBuf);
  Serial.println("[SD] === SD test task done ===");
  sdTestHandle = nullptr; 
  vTaskDelete(NULL);
}



// -------------------------
// Task: Stepper test
// -------------------------
// Helper: drive ENABLE according to polarity
static inline void driver_enable(bool on) {
  if (ENABLE_ACTIVE_HIGH) {
    digitalWrite(PIN_EN, on ? HIGH : LOW);
  } else {
    digitalWrite(PIN_EN, on ? LOW : HIGH);
  }
}

// Helper: start/stop STEP pulses at given frequency
// Duty = 50% so each period produces 2 edges (1 step per rising edge is fine).
static void step_pwm_start(uint32_t freq_hz) {
  // Set frequency first, then duty. Duty uses 8-bit scale by default.
  analogWriteFrequency(PIN_STEP, freq_hz);
  analogWrite(PIN_STEP, 128);  // ~50% duty (range 0..255)
}
static void step_pwm_stop() {
  analogWrite(PIN_STEP, 0);    // duty=0 -> output low (no pulses)
}

// The frequency sweep you want to test (Hz). Adjust as you like.
static const uint32_t kFreqList[] = { 10, 25, 50, 100, 250, 500, 1000, 2000, 5000, 10000 };
static const size_t   kFreqCount  = sizeof(kFreqList)/sizeof(kFreqList[0]);

// How long to run at each speed (ms)
static const uint32_t kRunMsPerStep = 1500;

// Minimal “are we e-stopped?” hook (placeholder).
// Replace this with your real E-stop read whenever you wire it into firmware.
static inline bool estop_active() {
  // e.g., read a pin: return digitalRead(PIN_ESTOP) == ESTOP_ACTIVE_LEVEL;
  return false;
}

void stepper_test_task(void* pv) {
  (void)pv;
  Serial.println("\n[MOTOR] === Stepper motor output test starting (single axis) ===");

  // --- Pin init ---
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR,  OUTPUT);
  pinMode(PIN_EN,   OUTPUT);

  // Put outputs in a safe idle state
  step_pwm_stop();
  digitalWrite(PIN_DIR, LOW);     // initial direction (LOW)
  driver_enable(false);

  // Small pause to settle
  vTaskDelay(pdMS_TO_TICKS(100));

  // --- Phase A: Direction LOW, sweep up the frequency list ---
  Serial.println("[MOTOR] Enable driver (phase A), DIR=LOW");
  driver_enable(true);
  // DIR setup time before first step pulses
  vTaskDelay(pdMS_TO_TICKS(1));   // 1 ms > 10 us requirement

  for (size_t i = 0; i < kFreqCount; ++i) {
    if (estop_active()) { Serial.println("[MOTOR][ABORT] E-STOP active."); break; }
    uint32_t f = kFreqList[i];
    Serial.printf("[MOTOR] Run at %lu Hz for %lu ms\n", (unsigned long)f, (unsigned long)kRunMsPerStep);
    step_pwm_start(f);
    vTaskDelay(pdMS_TO_TICKS(kRunMsPerStep));
  }

  // Stop pulses before changing direction
  step_pwm_stop();
  vTaskDelay(pdMS_TO_TICKS(50));

  // --- Phase B: Direction HIGH, sweep again ---
  Serial.println("[MOTOR] Change DIR=HIGH, re-sweep");
  digitalWrite(PIN_DIR, HIGH);
  vTaskDelay(pdMS_TO_TICKS(1));   // DIR setup time before next pulses

  for (size_t i = 0; i < kFreqCount; ++i) {
    if (estop_active()) { Serial.println("[MOTOR][ABORT] E-STOP active."); break; }
    uint32_t f = kFreqList[i];
    Serial.printf("[MOTOR] Run at %lu Hz for %lu ms\n", (unsigned long)f, (unsigned long)kRunMsPerStep);
    step_pwm_start(f);
    vTaskDelay(pdMS_TO_TICKS(kRunMsPerStep));
  }

  // --- Done: disable outputs ---
  step_pwm_stop();
  driver_enable(false);
  Serial.println("[MOTOR] Test complete. Driver disabled, STEP idle low.");

  // Mark task finished for CLI bookkeeping
  stepperTestHandle = nullptr;
  vTaskDelete(NULL);
}

// -------------------------
// Task: Gimbal 3 axis test 
// -------------------------
struct AxisCfg {
  int pin_step;               // PWM-capable pin
  int pin_dir;                // GPIO
  int pin_en;                 // GPIO
  bool enable_active_high;    // set false if ENABLE is active-LOW
  // Mechanics:
  uint16_t full_steps_per_rev;  // e.g., 200 for a 1.8° stepper
  uint16_t microsteps;          // e.g., 16 for 1/16
  float    gear_ratio;          // e.g., 1.0 if direct, 5.0 if 5:1 gearbox
};

// === EDIT THESE FOR YOUR THREE AXES ===
static AxisCfg AX[3] = {
  //   STEP  DIR   EN   EN_POL   steps  µsteps  gear
  {    3,    4,    2,   false,     200,    8,   1.0f }, // X Axis
  {    6,    5,    9,   false,     200,    8,   1.0f }, // Y Axis 
  {    29,    32,   30,   false,     200,    8,   1.0f }, // Z Axis 
};

// Test parameters (tune if needed)
static const float    MAX_DEG         = 90.0f;   // do not exceed 90°
static const uint32_t STEP_FREQ_HZ    = 200;    // common step rate for all axes
static const uint32_t DIR_SETUP_US    = 1000;    // 1 ms gap before/after DIR change
static const uint32_t SETTLE_MS       = 100;     // initial settle after enables 

// Helpers
static inline void drv_enable(const AxisCfg& a, bool on) {
  digitalWrite(a.pin_en, (a.enable_active_high ? (on ? HIGH : LOW)
                                               : (on ? LOW  : HIGH)));
}
static inline void step_start_pwm(const AxisCfg& a, uint32_t freq_hz) {
  analogWriteFrequency(a.pin_step, freq_hz);
  analogWrite(a.pin_step, 128); // ~50% duty
}
static inline void step_stop_pwm(const AxisCfg& a) {
  analogWrite(a.pin_step, 0);   // idle low
}
static inline uint32_t steps_for_degrees(const AxisCfg& a, float deg) {
  // steps = full_steps_per_rev * microsteps * gear_ratio * (deg / 360)
  const float steps = (float)a.full_steps_per_rev * (float)a.microsteps * a.gear_ratio * (deg / 360.0f);
  return (uint32_t)(steps + 0.5f); // round to nearest
}

// Start all axes towards target (dir), then stop each when its time is up
static void move_all_axes_timed(bool to_positive_limit) {
  // Direction per axis
  for (int i = 0; i < 3; ++i) {
    digitalWrite(AX[i].pin_dir, to_positive_limit ? HIGH : LOW);
  }
  // Respect DIR setup time
  delayMicroseconds(DIR_SETUP_US);

  // Compute durations from steps and common frequency
  uint32_t now_us = micros();
  bool active[3] = {false, false, false};
  uint32_t stop_at_us[3] = {0,0,0};

  for (int i = 0; i < 3; ++i) {
    const uint32_t steps = steps_for_degrees(AX[i], MAX_DEG); // 90°
    const float seconds  = (float)steps / (float)STEP_FREQ_HZ;
    const uint32_t dur_us = (uint32_t)(seconds * 1e6f + 0.5f);
    stop_at_us[i] = now_us + dur_us;

    step_start_pwm(AX[i], STEP_FREQ_HZ);
    active[i] = true;
  }

  // Supervisory loop: stop each axis when its timer elapses
  for (;;) {
    bool any = false;
    uint32_t t = micros();
    for (int i = 0; i < 3; ++i) {
      if (active[i] && (int32_t)(t - stop_at_us[i]) >= 0) {
        step_stop_pwm(AX[i]);
        active[i] = false;
      }
      any |= active[i];
    }
    if (!any) break;
    // Be nice to USB/RTOS
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void gimbal_3axis_test_task(void* pv) {
  (void)pv;
  Serial.println("\n[GIMBAL] === 3-axis 0↔90° simultaneous test ===");
  Serial.println("[GIMBAL] Ensure the gimbal is at 0° on all axes before starting!");
  Serial.printf("[GIMBAL] Using %.0f° limit and %lu steps/s common rate.\n",
                (double)MAX_DEG, (unsigned long)STEP_FREQ_HZ);

  // Pin init
  for (int i = 0; i < 3; ++i) {
    pinMode(AX[i].pin_step, OUTPUT);
    pinMode(AX[i].pin_dir,  OUTPUT);
    pinMode(AX[i].pin_en,   OUTPUT);
    step_stop_pwm(AX[i]);
    digitalWrite(AX[i].pin_dir, LOW); // define LOW as "toward 0°" conventionally
  }

  // Enable all drivers
  for (int i = 0; i < 3; ++i) drv_enable(AX[i], true);
  vTaskDelay(pdMS_TO_TICKS(SETTLE_MS));

  // ---- Phase A: move from 0° to +90° simultaneously ----
  Serial.println("[GIMBAL] Phase A: 0° → +90° (all axes)");
  move_all_axes_timed(/*to_positive_limit=*/true);

  // Small idle gap
  for (int i = 0; i < 3; ++i) step_stop_pwm(AX[i]);
  delayMicroseconds(DIR_SETUP_US);

  // ---- Phase B: move from +90° back to 0° simultaneously ----
  Serial.println("[GIMBAL] Phase B: +90° → 0° (all axes)");
  move_all_axes_timed(/*to_positive_limit=*/false);

  // Disable all drivers, stop STEP
  for (int i = 0; i < 3; ++i) {
    step_stop_pwm(AX[i]);
    drv_enable(AX[i], false);
  }
  Serial.println("[GIMBAL] Test complete. Drivers disabled.");

  gimbal3TestHandle = nullptr;
  vTaskDelete(NULL);
} 

// -------------------------
// Task: Rotary encoder test 
// -------------------------
// ===================== 3x Quadrature Encoder (no library) ====================
// Configure pins per axis (A, B, optional Z). Use Teensy digital pins that can
// be used with attachInterrupt (most can).
struct EncCfg {
  int pinA;
  int pinB;
  int pinZ;       // set to -1 if you don't have index wired
  bool usePullup; // true if you want INPUT_PULLUP (else external pullups already present)
  uint16_t ppr;   // pulses per revolution (electrical cycles/rev). Your part: 600
  float gear;     // gear ratio between motor shaft and joint. 1.0 if direct.
};

// === EDIT THESE to match your wiring ===
static EncCfg ENC[3] = {
  //   A   B    Z    PULLUP  PPR   GEAR
  {   22, 21,  -1,   false,  600,  1.0f }, // Axis 0
  {   17, 16,  -1,   false,  600,  1.0f }, // Axis 1
  {   40, 39,  -1,   false,  600,  1.0f }, // Axis 2
};

// Internal state per encoder
struct EncState {
  volatile int32_t  count = 0;  // 4x counts (A/B both edges)
  volatile uint8_t  last  = 0;  // last (A<<1|B)
  volatile uint32_t errs  = 0;  // illegal transitions
  volatile uint32_t zhits = 0;  // index hits (if Z wired)
};
static EncState ES[3];

// Lookup table for 4-bit transition (prev2bits<<2 | curr2bits):
// +1, -1,  0, or error (we count errors separately, treat as 0)
static inline int8_t quad_delta(uint8_t prev2, uint8_t curr2) {
  static const int8_t LUT[16] = {
    /*00→00*/  0, /*00→01*/ +1, /*00→10*/ -1, /*00→11*/  0,
    /*01→00*/ -1, /*01→01*/  0, /*01→10*/  0, /*01→11*/ +1,
    /*10→00*/ +1, /*10→01*/  0, /*10→10*/  0, /*10→11*/ -1,
    /*11→00*/  0, /*11→01*/ -1, /*11→10*/ +1, /*11→11*/  0
  };
  return LUT[(prev2 << 2) | curr2];
}

// We’ll share one ISR body for each axis using lambda-like wrappers
static void enc_isr_body(int idx) {
  const int a = digitalReadFast(ENC[idx].pinA);
  const int b = digitalReadFast(ENC[idx].pinB);
  const uint8_t curr = (uint8_t)((a << 1) | b);
  const uint8_t prev = ES[idx].last;
  ES[idx].last = curr;

  const int8_t d = quad_delta(prev, curr);
  if      (d ==  1) ES[idx].count++;
  else if (d == -1) ES[idx].count--;
  else if (d ==  0) { /* no change (bounce or same state) */ }
  else               { ES[idx].errs++; } // (we never actually get here with this LUT)
}

static void enc0_isr() { enc_isr_body(0); }
static void enc1_isr() { enc_isr_body(1); }
static void enc2_isr() { enc_isr_body(2); }

static void z_isr_body(int idx) { ES[idx].zhits++; }
static void z0_isr() { z_isr_body(0); }
static void z1_isr() { z_isr_body(1); }
static void z2_isr() { z_isr_body(2); }

// Convert counts to joint degrees (counts are 4x of PPR)
static inline float counts_to_deg(int idx, int32_t counts) {
  const float cpr4 = (float)ENC[idx].ppr * 4.0f; // 4x decode
  return (counts * 360.0f) / (cpr4 * ENC[idx].gear);
}

void encoder_test_task(void* pv) {
  (void)pv;
  Serial.println("\n[ENC] === Encoder test starting ===");
  Serial.println("[ENC] Move each joint; you should see counts, deg, and velocity.");

  // Pin init & attach interrupts
  for (int i = 0; i < 3; ++i) {
    pinMode(ENC[i].pinA, ENC[i].usePullup ? INPUT_PULLUP : INPUT);
    pinMode(ENC[i].pinB, ENC[i].usePullup ? INPUT_PULLUP : INPUT);
    if (ENC[i].pinZ >= 0) pinMode(ENC[i].pinZ, ENC[i].usePullup ? INPUT_PULLUP : INPUT);

    // Initialize last state before enabling ISRs to avoid a bogus first delta
    uint8_t curr = (digitalReadFast(ENC[i].pinA) << 1) | digitalReadFast(ENC[i].pinB);
    ES[i].last = curr;

    // Interrupts on both A and B edges for 4x counting
    attachInterrupt(digitalPinToInterrupt(ENC[i].pinA), (i==0)?enc0_isr:(i==1)?enc1_isr:enc2_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC[i].pinB), (i==0)?enc0_isr:(i==1)?enc1_isr:enc2_isr, CHANGE);

    if (ENC[i].pinZ >= 0) {
      attachInterrupt(digitalPinToInterrupt(ENC[i].pinZ), (i==0)?z0_isr:(i==1)?z1_isr:z2_isr, RISING);
    }
  }

  // Sampling for velocity
  const uint32_t print_every_ms = 200;
  int32_t prevCounts[3] = { ES[0].count, ES[1].count, ES[2].count };
  uint32_t t0 = millis();

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(print_every_ms));
    uint32_t t1 = millis();
    float dt_s = (t1 - t0) / 1000.0f;
    t0 = t1;

    for (int i = 0; i < 3; ++i) {
      int32_t c = ES[i].count;                     // atomic enough on ARM for 32-bit reads
      int32_t dc = c - prevCounts[i];
      prevCounts[i] = c;

      // Position in degrees and speed in deg/s
      float pos_deg = counts_to_deg(i, c);
      float vel_dps = counts_to_deg(i, dc) / dt_s;

      Serial.printf("[ENC%d] cnt=%ld  deg=%.2f  vel=%.2f deg/s  errs=%lu  z=%lu\n",
                    i, (long)c, (double)pos_deg, (double)vel_dps,
                    (unsigned long)ES[i].errs, (unsigned long)ES[i].zhits);
    }
    Serial.println();
  }
} 

// -------------------------
// Task: Encoder to SD Card Logger 
// -------------------------
// --- Logging control & stats ---
static volatile bool g_log_run = false;   // set true to start sampling & writing
static volatile bool g_log_stop = false;  // request a graceful stop
static volatile uint32_t g_log_drops = 0; // records dropped due to buffer full

// --- Sampling rate (Hz) ---
static const uint32_t LOG_HZ = 1000;   // 1 kHz sampling by default

// --- Record format (binary) ---
// Packed 4-byte aligned:  timestamp (us), enc0, enc1, enc2 (4 x int32_t)
// Size = 16 bytes per record
struct __attribute__((packed, aligned(4))) EncRecord {
  uint32_t t_us;
  int32_t  c0, c1, c2;
};

// --- Ring buffer ---
static const uint32_t RB_CAP = 4096;           // capacity (power of two OK but not required)
static EncRecord      RB[RB_CAP];              // storage
static volatile uint32_t rb_head = 0;          // write index (ISR)
static volatile uint32_t rb_tail = 0;          // read index (logger task)

// --- IO scratch for batched writes ---
static const uint32_t IO_CHUNK = 16 * 1024;    // 16 KB buffered writes
static uint8_t*       g_io_buf = nullptr;

// --- Sampling timer ---
#include <IntervalTimer.h>
static IntervalTimer  g_sampleTimer;

// Forward decls (you already have ES[] from the encoder test)
extern volatile int32_t ES_count0;  // if you *didn't* expose these, see note below
extern volatile int32_t ES_count1;
extern volatile int32_t ES_count2;
// If your encoder code keeps counts in ES[3].count, we’ll reference them directly in the sampler block below.

static inline uint32_t rb_free() {
  uint32_t h = rb_head, t = rb_tail;
  return (t + RB_CAP - h) % RB_CAP - 1; // one slot guard; returns max-1 in practice
}

static void sampler_isr() {
  if (!g_log_run) return;

  // Prepare record
  EncRecord rec;
  rec.t_us = micros();

  // Read current counts (atomic 32-bit reads are OK on M7 if not torn; ISR runs with interrupts off)
  // If your encoder state is ES[3].count, use those:
  extern EncState ES[3];
  rec.c0 = ES[0].count;
  rec.c1 = ES[1].count;
  rec.c2 = ES[2].count;

  // Push into ring buffer
  uint32_t h = rb_head;
  uint32_t n = (h + 1) % RB_CAP;
  if (n == rb_tail) {
    // buffer full -> drop
    g_log_drops++;
    return;
  }
  RB[h] = rec;
  rb_head = n;
}

// ===================== Logger task (consumer) =================================
static const char* LOG_TMP = "/enc_log.tmp";
static const char* LOG_BIN = "/enc_log.bin";

static void rb_drain_to_buf(uint8_t* out, uint32_t out_cap, uint32_t& out_len) {
  out_len = 0;
  while (rb_tail != rb_head) {
    if (out_len + sizeof(EncRecord) > out_cap) break;
    *((EncRecord*)(out + out_len)) = RB[rb_tail];
    out_len += sizeof(EncRecord);
    rb_tail = (rb_tail + 1) % RB_CAP;
  }
}

void encoder_logger_task(void* pv) {
  (void)pv;
  Serial.println("\n[LOG] Encoder logger starting…");

  // Mount SD (if not mounted yet). If you already mounted earlier, this should still succeed.
  if (!sd.begin(SdioConfig(FIFO_SDIO))) {
    Serial.println("[LOG][FAIL] sd.begin() — insert/format SD?");
    g_loggerTaskHandle = nullptr;
    vTaskDelete(NULL);
    return;
  }

  // Clean old artifacts
  if (sd.exists(LOG_TMP)) sd.remove(LOG_TMP);

  // Open temp file and write a tiny header for reference
  FsFile f;
  if (!f.open(LOG_TMP, O_RDWR | O_CREAT | O_TRUNC)) {
    Serial.println("[LOG][FAIL] open(tmp)");
    g_loggerTaskHandle = nullptr;
    vTaskDelete(NULL);
    return;
  }

  // Optional: simple ASCII header at start of file (fixed 128 bytes)
  const char* header = "ENCLOG v1 | rec= {t_us,uint32; c0,int32; c1,int32; c2,int32} | sample_hz=1000\n";
  f.write(header, strlen(header));

  // Allocate IO buffer
  g_io_buf = (uint8_t*)malloc(IO_CHUNK);
  if (!g_io_buf) {
    Serial.println("[LOG][FAIL] malloc IO_CHUNK");
    f.close();
    sd.remove(LOG_TMP);
    g_loggerTaskHandle = nullptr;
    vTaskDelete(NULL);
    return;
  }

  // Start sampling timer
  rb_head = rb_tail = 0;
  g_log_drops = 0;
  g_log_run   = true;
  g_log_stop  = false;
  g_sampleTimer.begin(sampler_isr, 1000000UL / LOG_HZ); // period in us

  Serial.println("[LOG] Logging… press 's' to stop");

  // Drain loop
  uint32_t flush_t0 = millis();
  for (;;) {
    // Drain any accumulated records to the IO buffer
    uint32_t chunk_len = 0;
    rb_drain_to_buf(g_io_buf, IO_CHUNK, chunk_len);

    if (chunk_len) {
      int wrote = f.write(g_io_buf, chunk_len);
      if (wrote != (int)chunk_len) {
        Serial.println("[LOG][FAIL] write()");
        break;
      }
    }

    // Flush to card every ~250 ms to reduce risk of data loss
    uint32_t now = millis();
    if (now - flush_t0 >= 250) {
      f.sync();
      flush_t0 = now;
    }

    // Graceful stop when requested and buffer is empty
    if (g_log_stop && rb_head == rb_tail) {
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // Stop sampling
  g_sampleTimer.end();
  g_log_run = false;

  // Final drain
  uint32_t tail_len = 0;
  rb_drain_to_buf(g_io_buf, IO_CHUNK, tail_len);
  if (tail_len) f.write(g_io_buf, tail_len);

  // Close and atomically promote
  f.sync();
  f.close();
  if (sd.exists(LOG_BIN)) sd.remove(LOG_BIN);
  if (!sd.rename(LOG_TMP, LOG_BIN)) {
    Serial.println("[LOG][WARN] rename(tmp->bin) failed; leaving .tmp");
  }

  // Report
  Serial.printf("[LOG] Stopped. Drops=%lu  File=%s\n",
                (unsigned long)g_log_drops, sd.exists(LOG_BIN) ? LOG_BIN : LOG_TMP);

  free(g_io_buf);
  g_io_buf = nullptr;
  g_loggerTaskHandle = nullptr;
  vTaskDelete(NULL);
}


// -------------------------
// Task: Print heartbeat message over USB Serial
// -------------------------
void serial_task(void *pvParameters)
{
  Serial.begin(115200);
  // Optional: wait up to 5s for the monitor to open
  unsigned long start = millis();
  while (!Serial && (millis() - start) < 5000) { vTaskDelay(pdMS_TO_TICKS(10)); }

  Serial.println("\n[CLI] Ready. Commands:");
  Serial.println("  h = help");
  Serial.println("  t = run SD test (mount, speed, CRC, atomic rename)");
  Serial.println("  m = run Stepper test (enable, DIR sweep, STEP freq sweep)");
  Serial.println("  g = run 3-axis 0↔90° simultaneous test"); 
  Serial.println("  e = run encoder test"); 
  Serial.println("  l = start encoder logging (binary, 1 kHz)");
  Serial.println("  s = stop encoder logging");
  Serial.println("  p = print logger stats");


  for (;;)
  {
    // Non-blocking read of any incoming byte
    if (Serial.available() > 0)
    {
      int c = Serial.read();

      // Ignore line endings from the monitor
      if (c == '\r' || c == '\n') continue;

      switch (c)
      {
        case 'h':
        case 'H':
          Serial.println("[CLI] Commands:");
          Serial.println("  h = help");
          Serial.println("  t = run SD test");
          Serial.println("  m = run Stepper test"); 
          Serial.println("  g = run 3-axis 0↔90° simultaneous test"); 
          Serial.println("  e = run encoder test"); 
          Serial.println("  l = start encoder logging (binary, 1 kHz)");
          Serial.println("  s = stop encoder logging");
          Serial.println("  p = print logger stats");
          break;

        case 't':
        case 'T':
          if (sdTestHandle == nullptr)
          {
            BaseType_t ok = xTaskCreate(sd_test_task, "SDTest", 4096, NULL, 1, &sdTestHandle);
            if (ok == pdPASS) Serial.println("[CLI] SD test started…");
            else              Serial.println("[CLI][ERR] Couldn't start SD test (stack/memory?)");
          }
          else
          {
            Serial.println("[CLI] SD test is already running.");
          }
          break;

        case 'm':
        case 'M':
          if (stepperTestHandle == nullptr)
          {
            BaseType_t ok = xTaskCreate(stepper_test_task, "MotorTest", 2048, NULL, 2, &stepperTestHandle);
            if (ok == pdPASS) Serial.println("[CLI] Stepper test started…");
            else              Serial.println("[CLI][ERR] Couldn't start Stepper test (stack/memory?)");
          }
          else
          {
            Serial.println("[CLI] Stepper test is already running.");
          }
          break;

        default:
          Serial.print("[CLI] Unknown key: ");
          Serial.write((char)c);
          Serial.println("  (press 'h' for help)");
          break;

        case 'g':
        case 'G':
          if (gimbal3TestHandle == nullptr) {
            BaseType_t ok = xTaskCreate(gimbal_3axis_test_task, "Gimbal3", 2048, NULL, 2, &gimbal3TestHandle);
            if (ok == pdPASS) Serial.println("[CLI] 3-axis 0↔90° test started…");
            else              Serial.println("[CLI][ERR] Couldn't start 3-axis test (stack/memory?)");
          } else {
            Serial.println("[CLI] 3-axis test already running.");
          }
          break;

        case 'e':
        case 'E':
          if (encTestHandle == nullptr) {
            BaseType_t ok = xTaskCreate(encoder_test_task, "EncTest", 2048, NULL, 2, &encTestHandle);
            if (ok == pdPASS) Serial.println("[CLI] Encoder test started…");
            else              Serial.println("[CLI][ERR] Couldn't start Encoder test (stack/memory?)");
          } else {
            Serial.println("[CLI] Encoder test already running.");
          }
          break;
        
        case 'l':   // start logging
        case 'L':
          if (g_loggerTaskHandle == nullptr) {
            // Avoid running SD speed test at the same time
            if (sdTestHandle != nullptr) {
              Serial.println("[CLI] SD speed test running; stop it before starting logger.");
              break;
            }
            BaseType_t ok = xTaskCreate(encoder_logger_task, "EncLog", 4096, NULL, 2, &g_loggerTaskHandle);
            if (ok == pdPASS) Serial.println("[CLI] Encoder logging started (1 kHz). File: enc_log.bin");
            else              Serial.println("[CLI][ERR] Couldn't start logger (stack/memory?)");
          } else {
            Serial.println("[CLI] Logger already running.");
          }
          break;

        case 's':   // stop logging
        case 'S':
          if (g_loggerTaskHandle != nullptr) {
            g_log_stop = true;  // logger will stop after draining buffer
            Serial.println("[CLI] Stop requested; waiting to flush…");
          } else {
            Serial.println("[CLI] Logger not running.");
          }
          break;

        case 'p':   // print quick stats
        case 'P':
          Serial.printf("[CLI] drops=%lu  rb_head=%lu rb_tail=%lu  running=%d stop=%d\n",
                        (unsigned long)g_log_drops,
                        (unsigned long)rb_head, (unsigned long)rb_tail,
                        (int)g_log_run, (int)g_log_stop);
          break;
      }
    }

    // When tasks end, they call vTaskDelete(NULL). We detect and clear handles.
    if (sdTestHandle && eTaskGetState(sdTestHandle) == eDeleted) {
      sdTestHandle = nullptr;
      Serial.println("[CLI] SD test finished.");
    }
    if (stepperTestHandle && eTaskGetState(stepperTestHandle) == eDeleted) {
      stepperTestHandle = nullptr;
      Serial.println("[CLI] Stepper test finished.");
    }
    if (gimbal3TestHandle && eTaskGetState(gimbal3TestHandle) == eDeleted) {
      gimbal3TestHandle = nullptr;
      Serial.println("[CLI] 3-axis test finished.");
    }
    if (encTestHandle && eTaskGetState(encTestHandle) == eDeleted) {
      encTestHandle = nullptr;
      Serial.println("[CLI] Encoder test finished.");
    }
  if (g_loggerTaskHandle && eTaskGetState(g_loggerTaskHandle) == eDeleted) {
    g_loggerTaskHandle = nullptr;
    Serial.println("[CLI] Encoder logger finished.");
  }


    vTaskDelay(pdMS_TO_TICKS(10)); // Be nice to the CPU/USB
  }
}



// -------------------------
// Arduino setup() — create tasks and start scheduler
// -------------------------
void setup()
{
  // Create LED task (stack size 256 words, priority 1)
  xTaskCreate(
    heartbeat_task,      // Task function
    "LED",               // Task name
    256,                 // Stack size (words, not bytes)
    NULL,                // Parameters
    1,                   // Priority (1 = low)
    NULL                 // Task handle (not needed here)
  );

  // Create Serial heartbeat task (stack size 512 words, priority 1)
  xTaskCreate(
    serial_task,
    "Serial",
    512,
    NULL,
    1,
    NULL
  );

  // Start the RTOS scheduler (never returns)
  vTaskStartScheduler();
}

// -------------------------
// Arduino loop() — unused with FreeRTOS
// -------------------------
void loop()
{
  // Empty — FreeRTOS now owns the CPU
}
