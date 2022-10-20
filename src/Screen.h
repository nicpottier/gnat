#pragma once
#include <Data.h>
#include <widget/Widget.h>

class Screen {
 public:
  Screen(ScreenID screenID)
      : m_screen{screenID} {};

  void tickAndPaint(data::Context ctx, TFT_eSPI& tft) {
    // if we aren't the active screen, noop
    if (ctx.screen != m_screen) {
      m_lastScreen = ctx.screen;
      return;
    }

    // otherwise, tick and paint all our widgets
    for (int i = 0; i < m_widgetCount; i++) {
      auto changed = m_widgets[i]->tick(ctx, ctx.tickID, millis());

      // repaint if the widget changed or we just switched screens
      if (changed || m_lastScreen != ctx.screen || !m_painted) {
        m_widgets[i]->paint(tft);
      }
    }

    if (ctx.screen != m_lastScreen) {
      m_lastScreen = ctx.screen;
    }

    if (!m_painted) {
      m_painted = true;
    }
  }

  void addWidget(widget::Widget* widget) {
    if (m_widgetCount == 10) {
      Serial.println("MAX WIDGETS PER SCREEN REACHED");
      return;
    }
    m_widgets[m_widgetCount] = widget;
    m_widgetCount++;
  }

 private:
  // have we ever been painted?
  bool m_painted = false;

  // what screen ID we hold widgets for
  ScreenID m_screen;

  // the last screen that was set in our tick
  ScreenID m_lastScreen;

  // the list of widgets we manage and their count
  widget::Widget* m_widgets[10];
  int m_widgetCount = 0;
};