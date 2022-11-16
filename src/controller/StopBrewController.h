#pragma once
#include <Context.h>
#include <controller/Controller.h>

namespace controller {

// Controller responsible for stopping our brew at the appropriate weight
class StopBrewController : public Controller {
 public:
  StopBrewController(QueueHandle_t cmdQ)
      : Controller(cmdQ) {}

  void tick(ctx::Context ctx) {
    // if we are brewing and within reach of our stop weight
    if (ctx.machineState == MachineState::espresso && ctx.machineSubstate == MachineSubstate::pouring &&
        ctx.currentWeight > ctx.config.getStopWeight() - 1) {
      if (ctx.tickID - m_lastStop > CMD_TIMEOUT) {
        // then send a stop cmd
        sendCommand(cmd::CommandRequest::newStopMachineCommand());
        m_lastStop = ctx.tickID;
      }
    }
  }

 private:
  long m_lastStop = 0;
};
}  // namespace controller