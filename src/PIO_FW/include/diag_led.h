#pragma once
#include <Arduino.h>

constexpr int LED_DIAG = LED_BUILTIN;

void ledOn();
void ledOff();
void blinkCode(int count);
