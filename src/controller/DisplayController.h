#pragma once

#include <functional>

namespace controller {

// the board display operations the controller drives, injected so its
// sequencing can be exercised off-device in tests
struct DisplayOps {
  std::function<void()> render;    // paint every screen and push the frame
  std::function<void()> powerOn;   // light the panel (frame must already be pushed)
  std::function<void()> powerOff;  // blank the panel
};

// owns the panel power state and, crucially, the order of render vs power. the
// invariant that keeps wake clean: when leaving sleep we render the new frame
// BEFORE lighting the panel, so it never flashes the stale pre-sleep frame.
// this runs in the render phase of the loop, after all screen/state logic, so
// the frame it paints reflects every decision made this tick.
class DisplayController {
 public:
  explicit DisplayController(DisplayOps ops)
      : m_ops(ops) {}

  // justSlept / justWoke are the machine sleep<->wake edges for this tick
  void render(bool justSlept, bool justWoke) {
    if (justSlept) {
      m_ops.powerOff();
      m_on = false;
      return;
    }

    // render whenever the panel is lit, or is about to be this tick
    if (m_on || justWoke) {
      m_ops.render();
    }

    // light the panel only after the fresh frame is in place
    if (justWoke && !m_on) {
      m_ops.powerOn();
      m_on = true;
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
