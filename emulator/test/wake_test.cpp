// headless emulator harness probing the sleep/wake display path. runs the
// real firmware (setup/loop from src/main.cpp against the shims) on a worker
// thread and drives the machine through the update queue like the BLE layer
// would, asserting on the display state the driver shim tracks.

#include <Arduino.h>

#include <Config.h>
#include <Data.h>

#include <cstdio>
#include <thread>

#include "../shim/emu_bridge.h"

extern void setup();
extern void loop();
extern data::Context g_ctx;

// queues are registered in creation order: found devices (0), updates (1),
// commands (2)
static QueueHandle_t updateQ() { return (QueueHandle_t)emu::queueAt(1); }
static void push(data::DataUpdate u) { xQueueSend(updateQ(), &u, 0); }
static void pump(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
static void setMachine(MachineState s, MachineSubstate ss) {
  push(data::DataUpdate::newMachineStateUpdate(s, ss));
}

static int failures = 0;
static void check(const char* what, bool cond) {
  printf("  %-44s %s\n", what, cond ? "ok" : "FAIL");
  if (!cond) failures++;
}

static const char* screenName(ScreenID s) {
  switch (s) {
    case ScreenID::brew: return "brew";
    case ScreenID::connect: return "connect";
    case ScreenID::config: return "config";
    case ScreenID::feedback: return "feedback";
    case ScreenID::adjust: return "adjust";
    case ScreenID::splash: return "splash";
    case ScreenID::pour: return "pour";
    default: return "unknown";
  }
}
static void state(const char* label) {
  printf("[%-16s] displayOn=%d screen=%-8s feedback=%d machineState=%d\n", label, (int)emu::displayOn,
         screenName(g_ctx.screen), (int)g_ctx.feedback, (int)g_ctx.machineState);
}

// sleep the machine, then wake it, asserting the panel comes back and gets a
// fresh frame. genUp injects the going_to_sleep intermediate the real DE1
// emits on the way down.
static void sleepWake(const char* label, bool goingToSleep) {
  if (goingToSleep) {
    setMachine(MachineState::going_to_sleep, MachineSubstate::ready);
    pump(80);
  }
  setMachine(MachineState::sleep, MachineSubstate::ready);
  pump(150);
  state(label);
  check("display off while asleep", emu::displayOn == false);

  long gen = emu::fbGeneration;
  setMachine(MachineState::idle, MachineSubstate::ready);
  pump(250);
  state(label);
  check("display back on after wake", emu::displayOn == true);
  check("panel got a fresh frame after wake", emu::fbGeneration != gen);
  // make sure it stays on (no immediate re-sleep)
  pump(300);
  check("display still on a beat later", emu::displayOn == true);
}

int main() {
  std::thread firmware([] {
    setup();
    loop();
  });
  firmware.detach();

  for (int i = 0; i < 200 && emu::queueAt(2) == nullptr; i++) pump(10);
  if (emu::queueAt(2) == nullptr) {
    printf("firmware never came up\n");
    return 1;
  }
  pump(100);

  push(data::DataUpdate::newConnectionStatus(DeviceType::machine, BLEState::connected));
  push(data::DataUpdate::newConnectionStatus(DeviceType::scale, BLEState::connected));
  push(data::DataUpdate::newWeightCommand(0));
  setMachine(MachineState::idle, MachineSubstate::ready);
  push(data::DataUpdate::newScreenUpdate(ScreenID::brew));  // skip boot splash
  pump(150);

  // pull a quick shot so the grind-feedback banner takes the screen
  setMachine(MachineState::espresso, MachineSubstate::preinfusing);
  pump(60);
  setMachine(MachineState::espresso, MachineSubstate::pouring);
  pump(60);
  setMachine(MachineState::espresso, MachineSubstate::ending);
  pump(60);
  setMachine(MachineState::idle, MachineSubstate::ready);
  pump(150);
  state("after shot");
  check("grind feedback banner is up", g_ctx.screen == ScreenID::feedback && g_ctx.feedback != FeedbackType::none);

  printf("\n-- scenario 1: sleep/wake from feedback (direct sleep) --\n");
  sleepWake("s1", false);

  printf("\n-- scenario 2: sleep/wake again (double cycle) --\n");
  sleepWake("s2", false);

  printf("\n-- scenario 3: sleep/wake with going_to_sleep intermediate --\n");
  sleepWake("s3", true);

  printf("\n-- scenario 4: sleep/wake from the adjust screen --\n");
  push(data::DataUpdate::newScreenUpdate(ScreenID::adjust));
  pump(120);
  sleepWake("s4", true);

  // scenario 5: a dropped wake notification. the machine wakes but its
  // sleep->idle notification never arrives, so nothing is queued and the
  // display stays dark. this is what DE1::refreshState (polled from bleLoop)
  // recovers by re-reading the state characteristic and queuing the change.
  printf("\n-- scenario 5: dropped wake notification, recovered by a state re-read --\n");
  setMachine(MachineState::sleep, MachineSubstate::ready);
  pump(150);
  check("display off while asleep", emu::displayOn == false);
  // the wake notification is lost: no update is delivered
  pump(250);
  check("display still dark with no wake signal", emu::displayOn == false);
  // the periodic state re-read surfaces the idle the notification missed
  setMachine(MachineState::idle, MachineSubstate::ready);
  pump(250);
  check("state re-read recovers the display", emu::displayOn == true);

  printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
