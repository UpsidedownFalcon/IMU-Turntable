#pragma once
#include <Arduino.h>
#include <SdFat.h>

// Single global SdFs object (Teensy 4.1 SDIO)
extern SdFs SD;

// Initialize SD card (mount filesystem)
bool sd_begin();

// Utility: check if card mounted successfully
bool sd_isReady();
