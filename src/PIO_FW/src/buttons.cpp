#include "buttons.h"
#include "io_pins.h"
#include "axis.h"
#include "config.h"

volatile RunState gRunState = RunState::Idle;
static uint16_t debounce_ms = 20;
static uint32_t last_ms_pp=0, last_ms_rs=0, last_ms_es=0;

void buttonsInit(uint16_t db){
  debounce_ms = db;
}

static bool pressed(uint8_t pin){
  if (pin == 255 || pin == 0xFF) return false;  // disabled
  return digitalReadFast(pin)==LOW;
}

void estopLatch(){
  gRunState = RunState::Estopped;
  axisStopAll();
  axisEnableAll(false);
}

void estopClear(){
  if (!gCfg.safety.estop_latching){
    gRunState = RunState::Idle;
  }
}

void buttonsPoll(){
  uint32_t now = millis();

  // E-stop (level sensitive)
  if (pressed(gPins.buttons.estop)){
    if (now - last_ms_es > debounce_ms){
      estopLatch();
      last_ms_es = now;
    }
  }

  if (gRunState == RunState::Estopped) return;

  // Play/Pause toggle
  if (pressed(gPins.buttons.play_pause) && (now - last_ms_pp > debounce_ms)){
    if (gRunState == RunState::Running) gRunState = RunState::Paused;
    else gRunState = RunState::Running;
    last_ms_pp = now;
  }

  // Reset (go to beginning, stop)
  if (pressed(gPins.buttons.reset) && (now - last_ms_rs > debounce_ms)){
    gRunState = RunState::Idle;
    last_ms_rs = now;
  }
}
