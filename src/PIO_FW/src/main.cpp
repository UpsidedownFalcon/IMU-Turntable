#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <Encoder.h>
#include <SdFat.h>

#include "io_pins.h"
#include "config.h"
#include "trajectory.h"
#include "axis.h"
#include "logging.h"
#include "buttons.h"
#include "telemetry.h"
#include "sd_backend.h"
#include "diag_led.h" 



// Encoders (constructed after pins applied, but Teensy Encoder tolerates this at global scope)
Encoder enc0(gPins.enc[0].a, gPins.enc[0].b);
Encoder enc1(gPins.enc[1].a, gPins.enc[1].b);
Encoder enc2(gPins.enc[2].a, gPins.enc[2].b);

// ===== Per-axis plan state =====
struct AxisPlan {
  TrajectoryReader tr;
  TrajHeader hdr{};
  uint64_t idx = 0;                 // current sample index
  uint32_t dt_us = 5000;            // frame dt for this axis
  uint32_t next_due_us = 0;         // next micros() time to read/schedule
  int32_t  last_cmd_deg1e6 = 0;     // last commanded angle
  int32_t  last_steps = 0;          // last absolute steps
  bool     done = false;
};
static AxisPlan plan[3];

static bool sd_ok=false, cfg_ok=false, traj_ok=false;

static TaskHandle_t sdTaskHandle, uiTaskHandle, hostTaskHandle, logTaskHandle, playTaskHandle;

static void setStatusLED(uint8_t on){ digitalWriteFast(gPins.leds.status, on?HIGH:LOW); }

static void progressBar(float pct){
  pct = constrain(pct,0,100);
  int n = (int)roundf(pct/20.0f); // 0..5
  for (int i=0;i<5;i++) digitalWriteFast(gPins.leds.progress[i], (i<n)?HIGH:LOW);
}

// ===== Helpers =====
static inline int32_t deg1e6_to_steps(int axis, int32_t deg1e6, uint32_t scale){
  float deg = (float)deg1e6 / (float)scale;
  float spr = (float)gCfg.stepper[axis].steps_per_rev * (float)gCfg.stepper[axis].microstep;
  float steps = deg * spr / 360.0f;
  return (int32_t)lroundf(steps);
}

static float overallProgressPct(){
  float p[3];
  for(int i=0;i<3;i++){
    if (plan[i].hdr.total_samples == 0) p[i] = plan[i].done ? 100.f : 0.f;
    else p[i] = 100.0f * (float)plan[i].idx / (float)plan[i].hdr.total_samples;
  }
  return (p[0] + p[1] + p[2]) / 3.0f;
}

// ===== Tasks =====

void sdTask(void*){
  pinMode(gPins.leds.status, OUTPUT);
  setStatusLED(1); // show "working"

  blinkCode(1); // boot -> sdTask reached 
  vTaskDelay(pdMS_TO_TICKS(500)); // wait for SD 

  if (!sd_begin()){
    sd_ok=false; setStatusLED(1); ledOn(); vTaskSuspend(nullptr);
  }
  sd_ok=true;

  blinkCode(2); // SD mounted 

  if (!loadConfigFromSD()){ cfg_ok=false; setStatusLED(1); ledOn(); vTaskSuspend(nullptr); }
  cfg_ok=true;
  applyPinModes(); // apply final pins after loading config 

  blinkCode(3); // Config OK

  axisEnableAll(false);  // ensure disabled until Running

  if (!validateTrajectories()){ traj_ok=false; setStatusLED(1); ledOn(); vTaskSuspend(nullptr); } 

  blinkCode(4); // Trajectories OK

  // Open each per-axis file
  traj_ok = true;
  for(int i=0;i<3;i++){
    if (!plan[i].tr.open(gCfg.ui.trajectory_files[i]) || !plan[i].tr.ok()){ traj_ok=false; break; }
    plan[i].tr.readHeader(plan[i].hdr);
    plan[i].dt_us = plan[i].hdr.sample_dt_us;
    plan[i].idx = 0;
    plan[i].done = (plan[i].hdr.total_samples==0);
  }

  if (!traj_ok){ setStatusLED(1); vTaskSuspend(nullptr); }

  // Status LED "OK" (adapt level/polarity to your wiring)
  setStatusLED(0);
  Serial.println("SD card read"); 
  vTaskSuspend(nullptr);
}

void uiTask(void*){
  buttonsInit(gCfg.safety.debounce_ms);
  for(;;){
    buttonsPoll();

    // Progress LED bar
    progressBar(overallProgressPct());

    vTaskDelay(pdMS_TO_TICKS(5)); // ~200 Hz poll
  }
}

void hostLinkTask(void*){
  telemetryInit(gCfg.telemetry.serial_baud);
  telemetrySendState("BOOT");
  for(;;){
    telemetryTick();
    if (telemetryHostActive()){
      telemetrySendEncoders((int32_t)enc0.read(), (int32_t)enc1.read(), (int32_t)enc2.read());
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void loggerTask(void*){
  if (!gCfg.logging.enabled){ vTaskSuspend(nullptr); }
  if (!logOpen(gCfg.logging.rate_hz)){ vTaskSuspend(nullptr); }
  const uint32_t period_us = 1000000UL / max<uint16_t>(1, gCfg.logging.rate_hz);
  TickType_t last = xTaskGetTickCount();
  const TickType_t dt = pdUS_TO_TICKS(period_us);

  for(;;){
    LogSample s{};
    s.t_us = micros();
    s.enc[0] = (int32_t)enc0.read();
    s.enc[1] = (int32_t)enc1.read();
    s.enc[2] = (int32_t)enc2.read();
    s.cmd_deg_x1e6[0] = plan[0].last_cmd_deg1e6;
    s.cmd_deg_x1e6[1] = plan[1].last_cmd_deg1e6;
    s.cmd_deg_x1e6[2] = plan[2].last_cmd_deg1e6;
    logWrite(s);

    vTaskDelayUntil(&last, dt);
  }
}

void playbackTask(void*){
  // Wait for sdTask to finish setup
  while (!traj_ok) vTaskDelay(pdMS_TO_TICKS(50));

  // Initialization
  axisEnableAll(false);
  for (int i=0;i<3;i++){
    plan[i].tr.seekSample(0);
    plan[i].idx = 0;
    plan[i].done = (plan[i].hdr.total_samples==0);
    plan[i].last_cmd_deg1e6 = 0;
    plan[i].last_steps = 0;
    plan[i].next_due_us = micros(); // ready immediately on start
  }

  if (gCfg.ui.autoplay_on_insert) gRunState = RunState::Running;

  for(;;){
    if (gRunState == RunState::Estopped){
      axisStopAll();
      axisEnableAll(false);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    if (gRunState == RunState::Idle){
      axisStopAll();
      axisEnableAll(false);
      // keep next_due aligned with current time so we start promptly on Play
      uint32_t now = micros();
      for (int i=0;i<3;i++) plan[i].next_due_us = now;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (gRunState == RunState::Paused){
      axisStopAll();
      axisEnableAll(true);
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    // Running
    blinkCode(5); 
    axisEnableAll(true);

    uint32_t now = micros();
    bool all_done = true;

    for (int i=0;i<3;i++){
      if (plan[i].done) continue;
      all_done = false;

      // If this axis is due for the next sample, pull & schedule it
      if ((int32_t)(now - plan[i].next_due_us) >= 0){
        int32_t cmd_deg1e6 = plan[i].last_cmd_deg1e6;
        if (!plan[i].tr.readNextScalar(i /*axis index used only for legacy files*/, cmd_deg1e6)){
          // On read error, stop gracefully
          plan[i].done = true;
          continue;
        }
        plan[i].last_cmd_deg1e6 = cmd_deg1e6;

        // Map absolute angle to absolute steps, schedule delta over this axis' dt
        int32_t abs_steps = deg1e6_to_steps(i, cmd_deg1e6, plan[i].tr.scale());
        int32_t delta = abs_steps - plan[i].last_steps;
        plan[i].last_steps = abs_steps;
        axisScheduleSteps(i, delta, plan[i].dt_us);

        // Telemetry for this axis
        // (Send all three to keep UI simple.)
        telemetrySendCmd(plan[0].last_cmd_deg1e6, plan[1].last_cmd_deg1e6, plan[2].last_cmd_deg1e6);

        // Advance schedule time and index
        plan[i].idx++;
        if (plan[i].idx >= plan[i].hdr.total_samples){
          plan[i].done = true;
        }
        plan[i].next_due_us += plan[i].dt_us;
        // If we fell behind a lot (e.g., after Pause), snap to now to avoid burst
        if ((int32_t)(now - plan[i].next_due_us) > (int32_t)plan[i].dt_us){
          plan[i].next_due_us = now + plan[i].dt_us;
        }
      }
    }

    // Send overall progress periodically
    telemetrySendProgress(overallProgressPct());

    if (all_done){
      gRunState = RunState::Idle; // finished
      continue;
    }

    // Yield a bit — choose a small sleep to keep latency low but avoid 100% CPU.
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ===== Setup / Loop =====

void setup(){
  applyPinModes();
  setStatusLED(1); // "working"

  pinMode(LED_BUILTIN, OUTPUT);
  ledOff(); 


  xTaskCreate(sdTask, "SD", 4096, nullptr, 3, &sdTaskHandle);
  xTaskCreate(uiTask, "UI", 2048, nullptr, 2, &uiTaskHandle);
  xTaskCreate(hostLinkTask, "Host", 4096, nullptr, 2, &hostTaskHandle);
  xTaskCreate(loggerTask, "Log", 4096, nullptr, 1, &logTaskHandle);
  xTaskCreate(playbackTask, "Play", 4096, nullptr, 2, &playTaskHandle);

  vTaskStartScheduler();
}

void loop(){ /* unused with FreeRTOS */ }
