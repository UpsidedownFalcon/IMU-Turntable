#pragma once
#include <Arduino.h>
#include <IntervalTimer.h>
#include "io_pins.h"

struct AxisRuntime {
  volatile int32_t steps_remaining = 0;
  volatile uint32_t half_period_us = 1000; // toggles every half-period
  volatile bool active = false;
  volatile bool step_level = false;
  volatile bool dir = false;
};

extern AxisRuntime gAxis[3];

void axisInit();
void axisEnableAll(bool enable);
void axisSetDir(int i, bool dir);
void axisScheduleSteps(int i, int32_t steps, uint32_t dt_us); // evenly over dt
void axisStopAll();
