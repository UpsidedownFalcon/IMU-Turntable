#pragma once
#include <Arduino.h>

enum class RunState : uint8_t { Idle, Running, Paused, Estopped };

extern volatile RunState gRunState;

void buttonsInit(uint16_t debounce_ms);
void buttonsPoll();  // called in uiTask at ~200 Hz
void estopLatch();
void estopClear();
