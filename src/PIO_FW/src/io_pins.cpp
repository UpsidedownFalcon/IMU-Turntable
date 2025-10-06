#include "io_pins.h"

PinConfig gPins = {
  // axis
  {
    {2,3,4,false,false,true},
    {5,6,7,false,false,true},
    {8,9,10,false,false,true}
  },
  // enc
  {
    {22,23,-1},{24,25,-1},{26,27,-1}
  },
  // buttons
  {30,31,32},
  // leds
  {33,{34,35,36,37,38}},
  // sd
  10
};

void applyPinModes() {
  for (int i=0;i<3;i++){
    pinMode(gPins.axis[i].en, OUTPUT);
    pinMode(gPins.axis[i].dir, OUTPUT);
    pinMode(gPins.axis[i].step, OUTPUT);
    digitalWrite(gPins.axis[i].en, gPins.axis[i].invert_en ? HIGH : LOW); // disabled by default
    digitalWrite(gPins.axis[i].dir, LOW);
    digitalWrite(gPins.axis[i].step, gPins.axis[i].step_active_high ? LOW : HIGH);
  }
  pinMode(gPins.leds.status, OUTPUT);
  for (int i=0;i<5;i++) pinMode(gPins.leds.progress[i], OUTPUT);

  pinMode(gPins.buttons.play_pause, INPUT_PULLUP);
  pinMode(gPins.buttons.reset, INPUT_PULLUP);
  pinMode(gPins.buttons.estop, INPUT_PULLUP);
}
