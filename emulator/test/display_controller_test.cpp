// unit test for DisplayController's clear/render-vs-power sequencing. the panel
// retains its last frame across sleep and ignores writes while asleep, so the
// property that keeps wake clean is: on sleep we blank the frame BEFORE
// powering off, and on wake we power on (onto that blank frame) before painting
// the real one. that way the panel never shows the stale pre-sleep screen.
//
// records the order of the injected ops as a string: R render, C clear,
// + power on, - power off.

#include <cstdio>
#include <string>

#include <controller/DisplayController.h>

static std::string calls;

static controller::DisplayController make() {
  return controller::DisplayController({
      [] { calls += 'R'; },
      [] { calls += 'C'; },
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

  // going to sleep blanks the frame BEFORE powering off, and does not render
  {
    auto d = make();
    calls.clear();
    d.render(true, false);
    expect("sleep clears then powers off", calls, "C-");
    // and while asleep, nothing happens
    calls.clear();
    d.render(false, false);
    expect("asleep steady does nothing", calls, "");
  }

  // waking powers on (onto the blanked frame) THEN paints the real frame
  {
    auto d = make();
    d.render(true, false);  // sleep first
    calls.clear();
    d.render(false, true);  // wake
    expect("wake powers on before rendering", calls, "+R");
    if (!d.isOn()) {
      printf("  %-46s FAIL (panel not on after wake)\n", "panel on after wake");
      failures++;
    }
  }

  // a full cycle: boot render, sleep (clear+off), asleep, wake (on+render),
  // awake render — the panel is never lit while holding the stale frame
  {
    auto d = make();
    calls.clear();
    d.render(false, false);  // boot/awake
    d.render(true, false);   // sleep
    d.render(false, false);  // asleep
    d.render(false, true);   // wake
    d.render(false, false);  // awake again
    expect("full sleep/wake cycle order", calls, "RC-+RR");
  }

  printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
