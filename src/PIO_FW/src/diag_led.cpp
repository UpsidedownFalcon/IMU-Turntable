#include "diag_led.h"

void ledOn()  { digitalWriteFast(LED_DIAG, HIGH); }
void ledOff() { digitalWriteFast(LED_DIAG, LOW);  }

int on_ms=150;
int off_ms=150;

void blinkCode(int count) {
  for (int i = 0; i < count; ++i) {
    ledOn();
    delay(on_ms);
    ledOff();
    delay(off_ms);
  }
  delay(500);  // pause between code repeats
}
