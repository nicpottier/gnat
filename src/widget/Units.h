#pragma once

#include <math.h>

namespace widget {

// values are stored metric (celsius, milliliters) and converted at display
// time when the units setting is imperial

inline int displayTemp(int celsius, bool imperial) {
  return imperial ? celsius * 9 / 5 + 32 : celsius;
}

inline const char* tempUnit(bool imperial) {
  return imperial ? "F" : "C";
}

inline int displayVol(int ml, bool imperial) {
  return imperial ? int(round(ml / 29.5735)) : ml;
}

inline const char* volUnit(bool imperial) {
  return imperial ? "oz" : "ml";
}

}  // namespace widget
