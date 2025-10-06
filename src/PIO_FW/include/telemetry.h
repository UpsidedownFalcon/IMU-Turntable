#pragma once
#include <Arduino.h>

void telemetryInit(uint32_t baud);
void telemetryTick();   // poll serial, send live data if enabled
void telemetrySendProgress(float pct);
void telemetrySendState(const char* s);
void telemetrySendEncoders(int32_t e0,int32_t e1,int32_t e2);
void telemetrySendCmd(int32_t d0,int32_t d1,int32_t d2);
bool telemetryHostActive(); // PC-control mode handshake
