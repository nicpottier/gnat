#pragma once

#include <widget/Icons.h>
#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

// full screen banner shown after a shot that missed the target time or when
// the tank needs water, the profile button dismisses it
class FeedbackBanner : public Widget {
 public:
  FeedbackBanner(int width, int height)
      : m_width{width},
        m_height{height} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
    // when previewing, show a sample water level
    auto waterLevel = ctx.feedbackPreview ? 7 : ctx.waterLevel;

    if (ctx.feedback != m_feedback ||
        (m_feedback == FeedbackType::add_water && waterLevel != m_waterLevel) ||
        ctx.config.finerIsLeft() != m_finerLeft || ctx.config.isFlipped() != m_flipped) {
      m_feedback = ctx.feedback;
      m_feedbackSeconds = ctx.feedbackSeconds;
      m_waterLevel = waterLevel;
      m_finerLeft = ctx.config.finerIsLeft();
      m_flipped = ctx.config.isFlipped();
      return true;
    }

    return false;
  }

  void paint(TFT_eSPI& tft) {
    if (m_feedback == FeedbackType::none) {
      return;
    }

    const char* msg;
    char buffer[16];
    uint32_t bg;
    uint32_t fg;

    if (m_feedback == FeedbackType::add_water) {
      msg = "Add Water";
      snprintf(buffer, 16, "%dmm left", m_waterLevel);
      bg = theme.water_color;
      fg = TFT_WHITE;
    } else if (m_feedback == FeedbackType::no_scale) {
      msg = "Connect Scale";
      snprintf(buffer, 16, "shot stopped");
      bg = theme.error_color;
      fg = TFT_WHITE;
    } else if (m_feedback == FeedbackType::nailed_it) {
      msg = "Nailed it!";
      snprintf(buffer, 16, "%0.1fs shot", m_feedbackSeconds);
      bg = theme.banner_good_color;
      fg = theme.bg_color;
    } else {
      auto finer = m_feedback == FeedbackType::grind_finer;
      msg = finer ? "Grind Finer" : "Grind Coarser";
      snprintf(buffer, 16, "%ds shot", int(round(m_feedbackSeconds)));
      bg = finer ? theme.banner_color : theme.banner_alt_color;
      fg = theme.bg_color;
    }

    tft.fillRect(0, 0, m_width, m_height, bg);

    // icon centered between the top of the display and the title text
    auto titleTop = m_height / 2 + px(6) - px(13);
    auto iconCy = titleTop / 2;

    if (m_feedback == FeedbackType::add_water) {
      drawDrop(tft, m_width / 2, iconCy, fg, bg);
    } else if (m_feedback == FeedbackType::no_scale) {
      drawDismiss(tft, m_width / 2, iconCy, px(15), fg, bg);
    } else if (m_feedback == FeedbackType::nailed_it) {
      drawCheck(tft, m_width / 2, iconCy, fg);
    } else {
      // finer and coarser rotate opposite ways
      auto left = (m_feedback == FeedbackType::grind_finer) ? m_finerLeft : !m_finerLeft;
      drawRotation(tft, m_width / 2, iconCy, px(17), !left, fg);
    }

    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(FONT_TITLE);
    tft.setTextColor(fg, bg);
    tft.drawString(msg, m_width / 2, m_height / 2 + px(6));

    tft.setFreeFont(FONT_SUB);
    tft.drawString(buffer, m_width / 2, m_height / 2 + px(38));
    tft.setTextDatum(TL_DATUM);

    // checkmark hint by the physical dismiss button
#if defined(TOUCH_CST816)
    // touch panels dismiss with the home button, no hint needed
#elif defined(COMBO_BUTTON_PIN)
    // single button boards can't swap roles, the button lands top right when
    // the device is flipped
    if (m_flipped) {
      drawDismiss(tft, m_width - px(20), px(20), px(11), fg, bg);
    } else {
      drawDismiss(tft, px(20), m_height - px(20), px(11), fg, bg);
    }
#else
    // two button boards swap roles so the dismiss button stays on the bottom
    if (m_flipped) {
      drawDismiss(tft, m_width - px(20), m_height - px(20), px(11), fg, bg);
    } else {
      drawDismiss(tft, px(20), m_height - px(20), px(11), fg, bg);
    }
#endif
  }

 private:
  int m_width;
  int m_height;

  FeedbackType m_feedback = FeedbackType::none;
  double m_feedbackSeconds = 0;
  int m_waterLevel = 0;
  bool m_finerLeft = true;
  bool m_flipped = false;
};

}  // namespace widget
