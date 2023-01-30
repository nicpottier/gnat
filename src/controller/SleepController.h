#pragma once
#include <Context.h>
#include <TFT_eSPI.h>
#include <board.h>
#include <controller/Controller.h>

namespace controller {

// Controller responsible for sleeping and waking the machine/scales
class SleepController : public Controller {
 public:
  SleepController(QueueHandle_t cmdQ, TFT_eSPI tft)
      : Controller("Sleep Controller", cmdQ),
        m_tft(tft),
        m_lastState(MachineState::unknown),
        m_lastSleep(0L),
        m_idleStart(0L) {}

  void tick(ctx::Context ctx) {
    // nothing to do if we don't know our machine state
    if (ctx.machineState == MachineState::unknown) {
      return;
    }

    // if we haven't brewed anything for a bit, sleep
    if (ctx.machineState == MachineState::idle && millis() - m_idleStart > ctx.config.getSleepTime() * 60 * 1000) {
      if (ctx.tickID - m_lastSleep > CMD_TIMEOUT) {
        sendCommand(cmd::CommandRequest::newSleepCommand());
        m_lastSleep = ctx.tickID;
      }
    }

    // we just went to sleep, turn off the screen
    if (ctx.machineState == MachineState::sleep && m_lastState != ctx.machineState) {
      turnOffDisplay(m_tft);

      // send a sleep command to our BLE devices
      sendCommand(cmd::CommandRequest::newSleepCommand());
    }

    // just left sleep, turn on screen
    if ((m_lastState == MachineState::sleep || m_lastState == MachineState::unknown) &&
        ctx.machineState != MachineState::sleep) {
      turnOnDisplay(m_tft);

      // restart out timer
      m_idleStart = millis();

      // send a wake command to our BLE devices
      sendCommand(cmd::CommandRequest::newWakeCommand());
    }

    // switching into idle, reset our timeout
    if (ctx.machineState == MachineState::idle && m_lastState != MachineState::idle) {
      m_idleStart = millis();
    }

    m_lastState = ctx.machineState;
  }

 private:
  TFT_eSPI m_tft;
  MachineState m_lastState;
  long m_lastSleep;
  long m_idleStart;
};
}  // namespace controller