#pragma once
#include <Context.h>
#include <controller/Controller.h>

namespace controller {

// TareController tares the scale when a brew starts (or we leave sleep)
class TareController : public Controller {
 public:
  TareController(QueueHandle_t cmdQ)
      : Controller("Tare Controller", cmdQ),
        m_lastTare(0),
        m_lastState(MachineState::unknown),
        m_lastSubstate(MachineSubstate::unknown) {}

  const unsigned long CMD_TIMEOUT = 100;

  void tick(ctx::Context ctx) {
    // if we just switched into brewing espresso or are brewing and just left preinfusion tare our scale
    if ((ctx.machineState != m_lastState && ctx.machineState == MachineState::espresso) ||
        (ctx.machineState == MachineState::espresso && m_lastSubstate < MachineSubstate::preinfusing &&
         ctx.machineSubstate >= MachineSubstate::preinfusing)) {
      if (ctx.tickID - m_lastTare > CMD_TIMEOUT) {
        sendCommand(cmd::CommandRequest::newTareScaleCommand());
        m_lastTare = ctx.tickID;
      }
    }

    m_lastState = ctx.machineState;
    m_lastSubstate = ctx.machineSubstate;
  }

 private:
  unsigned long m_lastTare;
  MachineState m_lastState;
  MachineSubstate m_lastSubstate;
};
}  // namespace controller