#include "sd_backend.h"

// Global filesystem object
SdFat SD; 

// Internal state flag
static bool sdReady = false;

bool sd_begin() {
  Serial.println("[sd] Initializing sd card...");

  // Add delay for sd power-up stability
  delay(300);  // or vTaskDelay(pdMS_TO_TICKS(500)) if inside a task

  if (!SD.begin(SdioConfig(FIFO_SDIO))) {
    Serial.println("[sd][FAIL] sd.begin(SdioConfig(FIFO_SDIO))");
    sdReady = false;
    return false;
  }

  Serial.println("[sd] Card mounted OK via sdIO FIFO");
  sdReady = true;
  return true;
}

bool sd_isReady() {
  return sdReady;
}
