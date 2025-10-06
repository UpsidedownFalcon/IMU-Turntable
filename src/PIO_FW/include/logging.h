#pragma once
#include <Arduino.h>
#include <SdFat.h>

struct LogHeader {
  uint32_t magic;    // 'GLG1'
  uint32_t rate_hz;
  uint64_t start_us;
};

struct LogSample {
  uint64_t t_us;
  int32_t enc[3];    // encoder counts (raw)
  int32_t cmd_deg_x1e6[3]; // commanded angles
};

bool logOpen(uint32_t rate_hz);
void logWrite(const LogSample& s);
void logClose();
