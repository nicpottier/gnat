#pragma once
#include <Context.h>
#include <controller/Controller.h>

namespace controller {

// Controller responsible for rebooting the ESP
class RebootController : public Controller {
 public:
  RebootController(QueueHandle_t cmdQ)
      : Controller("Reboot Controller", cmdQ) {}

  void tick(ctx::Context ctx) {
    // if it's time to reboot, do so
    if (ctx.restartTickID > 0 && ctx.tickID > ctx.restartTickID) {
      ESP.restart();
    }
  }
};
}  // namespace controller