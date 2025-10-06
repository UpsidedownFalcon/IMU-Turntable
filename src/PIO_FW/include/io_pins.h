#pragma once
#include <Arduino.h>

// Will be loaded/overridden by commands.json at runtime.
// Provide defaults so board is safe even without SD.
struct AxisPins {
  uint8_t en, dir, step;
  bool invert_en, invert_dir, step_active_high;
};

struct EncoderPins { int a, b, index; };

struct ButtonPins { uint8_t play_pause, reset, estop; };

struct LedsPins { uint8_t status; uint8_t progress[5]; };

struct PinConfig {
  AxisPins axis[3];
  EncoderPins enc[3];
  ButtonPins buttons;
  LedsPins leds;
  uint8_t sd_cs;
};

extern PinConfig gPins;
void applyPinModes();
