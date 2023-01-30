#pragma once
#include <Command.h>
#include <Context.h>
#include <Esp.h>

namespace controller {

static const unsigned long CMD_TIMEOUT = 100;

class Controller {
 public:
  virtual void tick(ctx::Context ctx) = 0;

 protected:
  Controller(QueueHandle_t cmdQ)
      : m_cmdQ(cmdQ) {}

  // queues the passed in command
  void sendCommand(cmd::CommandRequest cmd) {
    auto stop = cmd::CommandRequest::newStopMachineCommand();
    xQueueSend(m_cmdQ, &stop, 10);
  }

 private:
  QueueHandle_t m_cmdQ;
};
}  // namespace controller