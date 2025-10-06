#include <SdFat.h> 
#include "logging.h"
#include "sd_backend.h"

static FsFile sLog; 

bool logOpen(uint32_t rate_hz){
  if (sLog) sLog.close();
  SD.mkdir("/logs"); 
  char path[64];
  // simple rotating name:
  for (int i=1;i<100;i++){
    snprintf(path,sizeof(path),"/logs/%04d.bin", i);
    if (!SD.exists(path)) { sLog.open(path, O_RDWR | O_CREAT | O_TRUNC); break; }
  }
  if (!sLog) return false;
  LogHeader h{0x31474C47UL, rate_hz, (uint64_t)micros()};
  sLog.write((uint8_t*)&h, sizeof(h));
  sLog.sync();
  return true;
}

void logWrite(const LogSample& s){
  if (!sLog) return;
  sLog.write((uint8_t*)&s, sizeof(s));
}

void logClose(){
  if (sLog) sLog.close();
}
