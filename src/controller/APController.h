#pragma once
#include <Context.h>
#include <ap.h>
#include <board.h>
#include <controller/Controller.h>

namespace controller {

// Controller responsible for turning our AP on and off
class APController : public Controller {
 public:
  APController(ctx::Context *ctx, QueueHandle_t updateQ, QueueHandle_t cmdQ)
      : Controller("AP Controller", cmdQ),
        m_ctx(ctx),
        m_updateQ(updateQ),
        m_lastScreen(ScreenID::unknown) {}

  void tick(ctx::Context ctx) {
    // handle our AP state based on what screen we are on
    if (m_lastScreen == ScreenID::brew && (ctx.screen == ScreenID::connect || ctx.screen == ScreenID::config)) {
      startAP(m_ctx, m_updateQ);
    } else if (ctx.screen == ScreenID::brew && m_lastScreen != ScreenID::unknown && m_lastScreen != ScreenID::brew) {
      stopAP();
    }

    if (m_lastScreen != ctx.screen) {
      m_lastScreen = ctx.screen;
    }
  }

 private:
  ctx::Context *m_ctx;
  QueueHandle_t m_updateQ;
  ScreenID m_lastScreen;
};
}  // namespace controller