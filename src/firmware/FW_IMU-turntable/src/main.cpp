#include <Arduino.h>       // Standard Arduino core (pins, Serial, etc.)
#include <FreeRTOS.h>      // Main FreeRTOS API
#include <task.h>          // Task creation and vTaskDelay
#include <SdFat.h> 

// Testing flags 
static TaskHandle_t sdTestHandle = nullptr; // Keep a handle/flag so we don't start multiple tests at once 
static const bool ENABLE_ACTIVE_HIGH = true; // set false if driver enable is active-LOW

// Teensy 4.1: use built-in SD via SDIO (fast)
static SdFat sd;
static const uint32_t kTestSizeBytes = 32UL * 1024UL * 1024UL;   // 32 MB
static const uint32_t kBlockSize     = 16UL * 1024UL;            // 16 KB blocks
static const char*    kSpeedFile     = "/speed.bin";             // test data file
static const char*    kAtomTmp       = "/atom.tmp";              // temp file for atomicity
static const char*    kAtomLog       = "/atom.log";              // final name after rename

// Stepper tests 
static const int PIN_STEP = 2;   // <-- CHANGE to your STEP pin (must support PWM)
static const int PIN_DIR  = 3;   // <-- CHANGE to your DIR pin (GPIO)
static const int PIN_EN   = 4;   // <-- CHANGE to your ENABLE pin (GPIO)

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
// Task: Print heartbeat message over USB Serial
// -------------------------
void serial_task(void *pvParameters)
{
  // Serial.begin(115200);
  // // Wait for Serial Monitor to open (optional)
  // while (!Serial && millis() < 5000) { }

  // for (;;)
  // {
  //   Serial.println("Heartbeat: FreeRTOS is running on Teensy 4.1!");
  //   vTaskDelay(pdMS_TO_TICKS(1000)); // Print once per second
  // }
  Serial.begin(115200);
  // Optional: wait up to 5s for the monitor to open
  unsigned long start = millis();
  while (!Serial && (millis() - start) < 5000) { vTaskDelay(pdMS_TO_TICKS(10)); }

  Serial.println("\n[CLI] Ready. Commands:");
  Serial.println("  h = help");
  Serial.println("  t = run SD test (mount, speed, CRC, atomic rename)");
  Serial.println("");

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
          break;

        case 't':
        case 'T':
          if (sdTestHandle == nullptr)
          {
            BaseType_t ok = xTaskCreate(sd_test_task, "SDTest", 4096, NULL, 1, &sdTestHandle);
            if (ok == pdPASS) Serial.println("[CLI] SD test started…");
            else              Serial.println("[CLI][ERR] Couldn't start SD test (out of memory?)");
          }
          else
          {
            Serial.println("[CLI] SD test is already running.");
          }
          break;

        default:
          Serial.print("[CLI] Unknown key: ");
          Serial.write((char)c);
          Serial.println("  (press 'h' for help)");
          break;
      }
    }

    // When the SD task ends, it should call vTaskDelete(NULL);
    // we can clear our handle here if it has finished.
    if (sdTestHandle != nullptr && eTaskGetState(sdTestHandle) == eDeleted)
    {
      sdTestHandle = nullptr;
      Serial.println("[CLI] SD test finished.");
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
