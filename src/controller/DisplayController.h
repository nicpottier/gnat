#pragma once

#include <functional>

namespace controller {

// the board display operations the controller drives, injected so its
// sequencing can be exercised off-device in tests
struct DisplayOps {
  std::function<void()> render;    // paint every screen and push the frame
  std::function<void()> clear;     // push a blank frame (while the panel is lit)
  std::function<void()> powerOn;   // light the panel
  std::function<void()> powerOff;  // blank the panel
};

// owns the panel power state and the order of clear / render vs power. the
// panel retains its last frame across sleep and ignores frame writes while
// asleep, so it comes back up showing whatever it slept with. to avoid
// flashing the pre-sleep screen on wake we blank the frame while the panel is
// still lit, just before powering off; the panel then wakes onto a blank frame
// and we paint the real one (the splash) immediately after. runs in the render
// phase of the loop, after all screen/state logic.
class DisplayController {
 public:
  explicit DisplayController(DisplayOps ops)
      : m_ops(ops) {}

  // justSlept / justWoke are the machine sleep<->wake edges for this tick
  void render(bool justSlept, bool justWoke) {
    if (justSlept) {
      // blank the retained frame while we can still write it, then power off
      m_ops.clear();
      m_ops.powerOff();
      m_on = false;
      return;
    }

    // light the panel first (it comes up on the blank frame from sleep), then
    // paint the real frame over it
    if (justWoke && !m_on) {
      m_ops.powerOn();
      m_on = true;
    }

    if (m_on) {
      m_ops.render();
    }
  }

  bool isOn() const {
    return m_on;
  }

 private:
  DisplayOps m_ops;

  // the panel comes up lit at boot, the init sequence leaves it on
  bool m_on = true;
};

}  // namespace controller
