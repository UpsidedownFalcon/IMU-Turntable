#pragma once
#include <Arduino.h>

struct StepperAxisCfg {
  uint32_t steps_per_rev;
  uint32_t microstep;
  float max_speed_dps;
  float max_accel_dps2;
  uint16_t step_pulse_us;
};

struct EncoderAxisCfg { uint32_t cpr; uint8_t quad; char units[4]; };

struct Config {
  uint8_t schema_version;
  uint32_t timebase_hz;

  StepperAxisCfg stepper[3];
  bool enable_active_low;

  EncoderAxisCfg encoder[3];

  struct {
    bool estop_latching;
    uint16_t debounce_ms;
  } safety;

  struct {
    bool enabled;
    uint16_t rate_hz;
    bool binary;
  } logging;

  struct {
    // NEW: three separate trajectory files, one per axis
    char trajectory_files[3][96];
    bool autoplay_on_insert;
  } ui;

  struct {
    uint32_t serial_baud;
    bool live_stream;
  } telemetry;
};

extern Config gCfg;

bool loadConfigFromSD();          // reads /gimbal/commands.json
bool validateTrajectories();      // opens each trajectory pointed by gCfg.ui.trajectory_files[i]
