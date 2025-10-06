#include "axis.h"
#include "config.h"

AxisRuntime gAxis[3];
static IntervalTimer timers[3];

static void pulseISR0(){ auto &A=gAxis[0]; if(!A.active) return;
  digitalWriteFast(gPins.axis[0].step, A.step_level ^ gPins.axis[0].step_active_high);
  A.step_level = !A.step_level;
  if (A.step_level==false){ // count on falling edge
    if (--A.steps_remaining <= 0){ A.active=false; timers[0].end(); }
  }
}
static void pulseISR1(){ auto &A=gAxis[1]; if(!A.active) return;
  digitalWriteFast(gPins.axis[1].step, A.step_level ^ gPins.axis[1].step_active_high);
  A.step_level = !A.step_level;
  if (A.step_level==false){ if (--A.steps_remaining <= 0){ A.active=false; timers[1].end(); } }
}
static void pulseISR2(){ auto &A=gAxis[2]; if(!A.active) return;
  digitalWriteFast(gPins.axis[2].step, A.step_level ^ gPins.axis[2].step_active_high);
  A.step_level = !A.step_level;
  if (A.step_level==false){ if (--A.steps_remaining <= 0){ A.active=false; timers[2].end(); } }
}

void axisInit(){
  for (int i=0;i<3;i++){
    digitalWriteFast(gPins.axis[i].step, gPins.axis[i].step_active_high ? LOW : HIGH);
    pinMode(gPins.axis[i].step, OUTPUT);
    pinMode(gPins.axis[i].dir, OUTPUT);
    pinMode(gPins.axis[i].en, OUTPUT);
  }
}

void axisEnableAll(bool enable){
  for (int i=0;i<3;i++){
    bool level = gPins.axis[i].invert_en ? !enable : enable;
    // enable pin is usually active low; gCfg.enable_active_low controls polarity.
    level = gCfg.enable_active_low ? !enable : enable;
    digitalWriteFast(gPins.axis[i].en, level);
  }
}

void axisSetDir(int i, bool dir){
  gAxis[i].dir = dir ^ gPins.axis[i].invert_dir;
  digitalWriteFast(gPins.axis[i].dir, gAxis[i].dir ? HIGH : LOW);
}

static void startTimer(int i){
  switch(i){
    case 0: timers[0].begin(pulseISR0, gAxis[0].half_period_us); break;
    case 1: timers[1].begin(pulseISR1, gAxis[1].half_period_us); break;
    case 2: timers[2].begin(pulseISR2, gAxis[2].half_period_us); break;
  }
}

void axisScheduleSteps(int i, int32_t steps, uint32_t dt_us){
  if (steps==0 || dt_us==0){ gAxis[i].active=false; return; }
  axisSetDir(i, steps>0);
  int32_t n = abs(steps);
  // frequency = n / dt; period = dt / n; half-period = period/2 
  const uint32_t min_half = max<uint32_t>(1, gCfg.stepper[i].step_pulse_us / 2);
  uint32_t halfp = max<uint32_t>((uint32_t)min_half, (uint64_t)dt_us * 500ULL / (uint64_t)n);
  auto &A = gAxis[i];
  A.steps_remaining = n;
  A.half_period_us = halfp;
  A.active = true;
  startTimer(i);
}

void axisStopAll(){
  for (int i=0;i<3;i++){ gAxis[i].active=false; timers[i].end(); }
}
