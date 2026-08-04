#pragma once

#include <widget/Theme.h>
#include <widget/Widget.h>

namespace widget {

class BatteryStatus : public Widget {
 public:
  BatteryStatus(int right, int y)
      : m_right{right},
        m_y{y} {};

  bool tick(data::Context ctx, unsigned long tickID, unsigned long millis) {
#ifdef BATTERY_ADC_PIN
    // sample every ~5 seconds
    if (tickID % 250 == 0) {
      int mv = analogReadMilliVolts(BATTERY_ADC_PIN) * 2;
      Serial.printf("[BATT] %dmv\n", mv);

      // above ~4.3v we are seeing the charger, not a battery
      int percent = -1;
      if (mv <= 4300) {
        percent = constrain((mv - 3300) / 9, 0, 100);
      }

      if (percent != m_percent) {
        m_percent = percent;
        return true;
      }
    }

    // the shot timer clears our corner while brewing, repaint behind it
    if (m_percent >= 0 && tickID % 50 == 0) {
      return true;
    }
#endif
    return false;
  }

  void paint(TFT_eSPI& tft) {
    if (m_percent < 0) {
      return;
    }

    char buffer[8];
    snprintf(buffer, 8, "%d%%", m_percent);
    tft.setTextFont(2);
    tft.setTextColor(theme.text_color, theme.dash_bg_color);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(buffer, m_right, m_y);
    tft.setTextDatum(TL_DATUM);
  }

 private:
  int m_right;
  int m_y;

  int m_percent = -1;
};

}  // namespace widget
