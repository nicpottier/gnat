#pragma once
#include <Command.h>
#include <Context.h>
#include <Esp.h>

namespace controller {

static const unsigned long CMD_TIMEOUT = 100;

class Controller {
 public:
  virtual void tick(ctx::Context ctx) = 0;
  const char* getName() {
    return m_name;
  }

 protected:
  Controller(const char* name, QueueHandle_t cmdQ)
      : m_name(name),
        m_cmdQ(cmdQ) {}

  // queues the passed in command
  void sendCommand(cmd::CommandRequest cmd) {
    xQueueSend(m_cmdQ, &cmd, 10);
  }

 private:
  QueueHandle_t m_cmdQ;
  const char* m_name;
};
}  // namespace controller