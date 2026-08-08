#pragma once

namespace widget {

// temperatures are stored in celsius and converted at display time when the
// units setting is imperial

inline int displayTemp(int celsius, bool imperial) {
  return imperial ? celsius * 9 / 5 + 32 : celsius;
}

inline const char* tempUnit(bool imperial) {
  return imperial ? "F" : "C";
}

}  // namespace widget
