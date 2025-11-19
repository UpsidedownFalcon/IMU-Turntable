#include "config.h"
#include "io_pins.h"
#include <ArduinoJson.h>
#include <SdFat.h> 
#include "sd_backend.h" 

Config gCfg = {};

static bool readJson(const char* path, JsonDocument& doc){
  FsFile f; 
  if (!f.open(path, O_RDONLY)) return false;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  return !err;
}

bool loadConfigFromSD(){
  DynamicJsonDocument doc(8192);
  if (!readJson("/gimbal/commands.json", doc)) return false;

  gCfg.schema_version = doc["schema_version"] | 0;
  if (gCfg.schema_version != 1) return false;

  gCfg.timebase_hz = doc["device"]["timebase_hz"] | 1000000;

  // Pins
  for (int i=0;i<3;i++){
    auto a = doc["pins"]["axis"][i];
    gPins.axis[i].en = a["enable"];
    gPins.axis[i].dir = a["dir"];
    gPins.axis[i].step = a["step"];
    gPins.axis[i].invert_en = a["invert_enable"] | false;
    gPins.axis[i].invert_dir = a["invert_dir"] | false;
    gPins.axis[i].step_active_high = a["step_active_high"] | true;
  }
  for (int i=0;i<3;i++){
    auto e = doc["encoders"]["axes"][i];
    gPins.enc[i].a = doc["pins"]["encoders"][i]["a"];
    gPins.enc[i].b = doc["pins"]["encoders"][i]["b"];
    gPins.enc[i].index = doc["pins"]["encoders"][i]["index"] | -1;

    gCfg.encoder[i].cpr = e["cpr"] | 2048;
    gCfg.encoder[i].quad = e["quad"] | 4;
    const char* u = e["units"] | "deg";
    strncpy(gCfg.encoder[i].units, u, sizeof(gCfg.encoder[i].units));
  }

  gPins.buttons.play_pause = doc["pins"]["buttons"]["play_pause"];
  gPins.buttons.reset      = doc["pins"]["buttons"]["reset"];
  gPins.buttons.estop      = doc["pins"]["estop"] | doc["pins"]["buttons"]["estop"]; // allow legacy location

  gPins.leds.status = doc["pins"]["leds"]["status"];
  for (int i=0;i<5;i++) gPins.leds.progress[i] = doc["pins"]["leds"]["progress_bar"][i];

  gPins.sd_cs = doc["pins"]["sd_card_cs"] | 10;

  // Stepper cfg
  for (int i=0;i<3;i++){
    auto s = doc["stepper"]["axes"][i];
    gCfg.stepper[i].steps_per_rev = s["steps_per_rev"] | 200;
    gCfg.stepper[i].microstep     = s["microstep"]     | 16;
    gCfg.stepper[i].max_speed_dps = s["max_speed_dps"] | 180.0;
    gCfg.stepper[i].max_accel_dps2= s["max_accel_dps2"]| 360.0;
    gCfg.stepper[i].step_pulse_us = s["step_pulse_us"] | 3;
  }
  gCfg.enable_active_low = doc["stepper"]["enable_active_low"] | true;

  // Safety
  gCfg.safety.estop_latching = doc["safety"]["estop_latching"] | true;
  gCfg.safety.debounce_ms    = doc["safety"]["debounce_ms"]    | 20;

  // Logging & UI
  gCfg.logging.enabled  = doc["logging"]["enabled"]  | true;
  gCfg.logging.rate_hz  = doc["logging"]["rate_hz"]  | 200;
  gCfg.logging.binary   = doc["logging"]["binary"]   | true;

  // NEW: per-axis trajectory paths
  const char* def0 = "/gimbal/trajectory_X.traj";
  const char* def1 = "/gimbal/trajectory_Y.traj";
  const char* def2 = "/gimbal/trajectory_Z.traj";
  const char* t0 = doc["ui"]["trajectory_files"][0] | def0;
  const char* t1 = doc["ui"]["trajectory_files"][1] | def1;
  const char* t2 = doc["ui"]["trajectory_files"][2] | def2;
  strncpy(gCfg.ui.trajectory_files[0], t0, sizeof(gCfg.ui.trajectory_files[0]));
  strncpy(gCfg.ui.trajectory_files[1], t1, sizeof(gCfg.ui.trajectory_files[1]));
  strncpy(gCfg.ui.trajectory_files[2], t2, sizeof(gCfg.ui.trajectory_files[2]));
  gCfg.ui.autoplay_on_insert = doc["ui"]["autoplay_on_insert"] | false;

  gCfg.telemetry.serial_baud = doc["telemetry"]["serial_baud"] | 115200;
  gCfg.telemetry.live_stream = doc["telemetry"]["live_stream"] | true;

  return true;
}

bool validateTrajectories(){
  for (int i=0;i<3;i++){
    FsFile f; 
    if (!f.open(gCfg.ui.trajectory_files[i], O_RDONLY)) return false;
    uint8_t hdr[32];
    if (f.read(hdr, 32) != 32) { f.close(); return false; }
    // 'G','M','B','L'
    if (hdr[0]!=0x47 || hdr[1]!=0x4D || hdr[2]!=0x42 || hdr[3]!=0x4C) { f.close(); return false; }
    // We accept axis_count==1 (preferred) OR ==3 (legacy); version==1
    if (hdr[4]!=1 /*version*/){ f.close(); return false; }
    f.close();

    uint32_t sample_dt_us = *(uint32_t*)&hdr[8];
    uint64_t total_samples = *(uint64_t*)&hdr[12];
    uint32_t angle_scale   = *(uint32_t*)&hdr[20];
    if (sample_dt_us == 0 || total_samples == 0 || angle_scale == 0) { f.close(); return false; }

  }
  return true;
}
