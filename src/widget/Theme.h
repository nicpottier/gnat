#pragma once

class Theme {
 public:
  Theme(int bg_color, int text_color, int ble_color, int error_color, int weight_color, int water_color,
        int pressure_color, int temp_color, int dash_bg_color, int dash_border_color)
      : bg_color{uint32_t(bg_color)},
        text_color{uint32_t(text_color)},
        ble_color{uint32_t(ble_color)},
        error_color{uint32_t(error_color)},
        weight_color{uint32_t(weight_color)},
        water_color{uint32_t(water_color)},
        pressure_color{uint32_t(pressure_color)},
        temp_color{uint32_t(temp_color)},
        dash_bg_color{uint32_t(dash_bg_color)},
        dash_border_color{uint32_t(dash_border_color)} {}
  uint32_t bg_color;
  uint32_t text_color;
  uint32_t ble_color;
  uint32_t error_color;
  uint32_t weight_color;
  uint32_t water_color;
  uint32_t pressure_color;
  uint32_t temp_color;
  uint32_t dash_bg_color;
  uint32_t dash_border_color;
};

// note these are 16 bit RGB565
// see: https://chrishewett.com/blog/true-rgb565-colour-picker/
const Theme dark_theme = Theme{
    0x0000,  // bg_color
    0xe73c,  // text_color
    0x0b59,  // ble_color
    0xb945,  // error_color
    0x9A60,  // weight_color
    0x001F,  // water_color
    0x4c87,  // pressure_color
    0xb945,  // temp_color
    0x823,   // dash_bg_color
    0x001,   // dash_border_color
};

const auto theme = dark_theme;