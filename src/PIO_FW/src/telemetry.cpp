#include "telemetry.h"

static bool hostActive=false;
static bool live=true;

void telemetryInit(uint32_t /*baud*/){
  // USB Serial on Teensy ignores baud.
  Serial.begin(115200);
}

void telemetryTick(){
  while (Serial.available()){
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line=="HELLO") { Serial.println("ACK"); hostActive=true; }
    else if (line=="LIVE_ON") live=true;
    else if (line=="LIVE_OFF") live=false;
    else if (line=="ESTOP") { Serial.println("ESTOP_ACK"); } // hook if you want remote estop
    // You can extend with commands to upload files to SD etc.
  }
}

void telemetrySendProgress(float pct){
  if (hostActive) { Serial.print("PROGRESS "); Serial.println(pct,2); }
}

void telemetrySendState(const char* s){
  if (hostActive) { Serial.print("STATE "); Serial.println(s); }
}

void telemetrySendEncoders(int32_t e0,int32_t e1,int32_t e2){
  if (hostActive){ Serial.printf("ENC %ld %ld %ld\n", (long)e0,(long)e1,(long)e2); }
}

void telemetrySendCmd(int32_t d0,int32_t d1,int32_t d2){
  if (hostActive){ Serial.printf("CMD %ld %ld %ld\n", (long)d0,(long)d1,(long)d2); }
}

bool telemetryHostActive(){ return hostActive; }
