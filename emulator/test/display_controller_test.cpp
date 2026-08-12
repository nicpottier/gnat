// unit test for DisplayController's render-vs-power sequencing. the property
// that keeps wake clean is that on a wake edge the frame is rendered BEFORE the
// panel is powered on, so it never lights up on the stale pre-sleep frame.
//
// records the order of the injected ops as a string: R render, + power on,
// - power off.

#include <cstdio>
#include <string>

#include <controller/DisplayController.h>

static std::string calls;

static controller::DisplayController make() {
  return controller::DisplayController({
      [] { calls += 'R'; },
      [] { calls += '+'; },
      [] { calls += '-'; },
  });
}

static int failures = 0;
static void expect(const char* what, const std::string& got, const std::string& want) {
  bool ok = got == want;
  printf("  %-46s %-8s %s\n", what, ok ? "ok" : "FAIL", ok ? "" : ("(got '" + got + "' want '" + want + "')").c_str());
  if (!ok) failures++;
}

int main() {
  // boot: panel already lit, steady awake tick just renders
  {
    auto d = make();
    calls.clear();
    d.render(/*justSlept=*/false, /*justWoke=*/false);
    expect("awake steady renders", calls, "R");
  }

  // going to sleep blanks the panel and does not render
  {
    auto d = make();
    calls.clear();
    d.render(true, false);
    expect("sleep powers off, no render", calls, "-");
    // and while asleep, nothing happens
    calls.clear();
    d.render(false, false);
    expect("asleep steady does nothing", calls, "");
  }

  // waking renders the fresh frame, THEN lights the panel
  {
    auto d = make();
    d.render(true, false);  // sleep first
    calls.clear();
    d.render(false, true);  // wake
    expect("wake renders before power on", calls, "R+");
    if (!d.isOn()) {
      printf("  %-46s FAIL (panel not on after wake)\n", "panel on after wake");
      failures++;
    }
  }

  // a full cycle: boot render, sleep, asleep, wake, awake render
  {
    auto d = make();
    calls.clear();
    d.render(false, false);  // boot/awake
    d.render(true, false);   // sleep
    d.render(false, false);  // asleep
    d.render(false, true);   // wake
    d.render(false, false);  // awake again
    expect("full sleep/wake cycle order", calls, "R-R+R");
  }

  printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
