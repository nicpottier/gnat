// the gnat emulator host: runs the real firmware (setup/loop from
// src/main.cpp compiled against the shims) on a worker thread, shows its
// panel in one SDL window and the simulated kitchen in another: a ghc
// cluster mirroring the machine's controls plus device toggles, with the
// mouse feeding the panel window as touch

#include <Arduino.h>

#include <Config.h>
#include <Data.h>
#include <Command.h>

#include <SDL.h>
#include <TFT_eSPI.h>
#include <unistd.h>

#include <thread>

#include "../shim/emu_bridge.h"

extern void setup();
extern void loop();

// the machine window layout: vertical, the ghc cluster on top with button
// pairs below it no wider than the pad itself
const int ctrl_w = 180;
const int ctrl_h = 344;

// the device window: the panel with a slim strip of device inputs below
const int strip_h = 38;

// the screen sits inset on a light gray body with a border so its
// boundaries are obvious
const int dev_margin = 16;
const int dev_w = emu_panel_w + dev_margin * 2;
const int dev_h = emu_panel_h + dev_margin * 2 + strip_h;
const uint16_t dev_body_color = 0xD69A;
const uint16_t dev_border_color = 0x632C;

// control ids
enum {
  BTN_SLEEP,
  BTN_IDLE,
  BTN_SHOT,
  BTN_STEAM,
  BTN_WATER,
  BTN_FLUSH,
  BTN_STOP,
  BTN_DE1,
  BTN_SCALE,
  BTN_TANK_DOWN,
  BTN_TANK_UP,
  BTN_SWIPE_L,
  BTN_SWIPE_R,
  BTN_HOME,
  BTN_DEVICE,
};

struct Button {
  int x, y, w, h;
  const char* label;
  int id;
};

// machine window: the simulated kitchen, two across under the ghc
static Button machineButtons[] = {
    {8, 184, 80, 28, "Sleep", BTN_SLEEP},      {92, 184, 80, 28, "Idle", BTN_IDLE},
    {8, 220, 80, 28, "DE1", BTN_DE1},          {92, 220, 80, 28, "Scale", BTN_SCALE},
    {8, 256, 80, 28, "Tank -", BTN_TANK_DOWN}, {92, 256, 80, 28, "Tank +", BTN_TANK_UP},
};
const int machine_button_count = sizeof(machineButtons) / sizeof(machineButtons[0]);

// device window strip: only inputs the physical device has
const int strip_y = emu_panel_h + dev_margin * 2 + 5;
static Button deviceButtons[] = {
    {16, strip_y, 120, 28, "Btn (hold)", BTN_DEVICE},
    {150, strip_y, 100, 28, "Home", BTN_HOME},
    {286, strip_y, 120, 28, "< Swipe", BTN_SWIPE_L},
    {418, strip_y, 120, 28, "Swipe >", BTN_SWIPE_R},
};
const int device_button_count = sizeof(deviceButtons) / sizeof(deviceButtons[0]);

// the ghc cluster mirroring the real machine: hot water up top, steam on
// the right, espresso at the bottom, flush on the left, stop in the middle
const int ghc_cx = ctrl_w / 2;
const int ghc_cy = 92;
const int ghc_pad_r = 82;
const int ghc_orbit = 52;
const int ghc_btn_r = 23;

struct GhcButton {
  int dx, dy;
  int id;
};

static GhcButton ghcButtons[] = {
    {0, -ghc_orbit, BTN_WATER}, {ghc_orbit, 0, BTN_STEAM}, {0, ghc_orbit, BTN_SHOT},
    {-ghc_orbit, 0, BTN_FLUSH}, {0, 0, BTN_STOP},
};
const int ghc_button_count = sizeof(ghcButtons) / sizeof(ghcButtons[0]);

// a scripted touch gesture playing out over render frames
struct Gesture {
  bool active = false;
  bool home = false;
  int frame = 0;
  int frames = 0;
  int fromX = 0, toX = 0, y = 0;
};

// the simulated DE1 and skale
struct Sim {
  bool de1 = false;
  bool scale = false;
  MachineState state = MachineState::sleep;
  MachineSubstate substate = MachineSubstate::ready;
  double weight = 0;
  int tank = 25;
  unsigned long phaseStart = 0;
  unsigned long lastEmit = 0;
  int sampleTime = 0;
  int frameNumber = 0;

  QueueHandle_t updateQ() { return emu::queueAt(1); }
  QueueHandle_t cmdQ() { return emu::queueAt(2); }

  void push(data::DataUpdate u) {
    auto q = updateQ();
    if (q) {
      xQueueSend(q, &u, 0);
    }
  }

  void setState(MachineState s, MachineSubstate ss) {
    state = s;
    substate = ss;
    phaseStart = millis();
    push(data::DataUpdate::newMachineStateUpdate(s, ss));
  }

  void connectDe1(bool on) {
    de1 = on;
    push(data::DataUpdate::newConnectionStatus(DeviceType::machine, on ? BLEState::connected : BLEState::disconnected));
    if (on) {
      setState(MachineState::idle, MachineSubstate::ready);
      pushTank();
    }
  }

  void connectScale(bool on) {
    scale = on;
    push(data::DataUpdate::newConnectionStatus(DeviceType::scale, on ? BLEState::connected : BLEState::disconnected));
    if (on) {
      pushWeight();
    }
  }

  void pushWeight() { push(data::DataUpdate::newWeightCommand(weight)); }
  void pushTank() { push(data::DataUpdate::newWaterLevelUpdate(tank, 3)); }

  void startShot() {
    if (!de1) {
      connectDe1(true);
    }
    frameNumber = 0;
    setState(MachineState::espresso, MachineSubstate::preinfusing);
  }

  void stopFlow() {
    if (state == MachineState::espresso) {
      setState(MachineState::espresso, MachineSubstate::ending);
    } else if (state == MachineState::steam || state == MachineState::hot_water ||
               state == MachineState::hot_water_rinse) {
      setState(MachineState::idle, MachineSubstate::ready);
    }
  }

  // ghc buttons behave like the machine's: start their flow, or stop it if
  // it's already running
  void ghcPress(int id) {
    if (!de1) {
      connectDe1(true);
    }
    switch (id) {
      case BTN_WATER:
        state == MachineState::hot_water ? stopFlow() : setState(MachineState::hot_water, MachineSubstate::pouring);
        break;
      case BTN_STEAM:
        state == MachineState::steam ? stopFlow() : setState(MachineState::steam, MachineSubstate::steaming);
        break;
      case BTN_SHOT:
        state == MachineState::espresso ? stopFlow() : startShot();
        break;
      case BTN_FLUSH:
        state == MachineState::hot_water_rinse
            ? stopFlow()
            : setState(MachineState::hot_water_rinse, MachineSubstate::pouring);
        break;
      case BTN_STOP:
        stopFlow();
        break;
    }
  }

  bool flowActiveFor(int id) {
    switch (id) {
      case BTN_WATER:
        return state == MachineState::hot_water;
      case BTN_STEAM:
        return state == MachineState::steam;
      case BTN_SHOT:
        return state == MachineState::espresso;
      case BTN_FLUSH:
        return state == MachineState::hot_water_rinse;
    }
    return false;
  }

  void emitSample(double pressure, double flow, double targetP, double targetF) {
    data::Sample s{};
    sampleTime += 100;
    s.sampleTime = sampleTime;
    s.groupPressure = pressure;
    s.groupFlow = flow;
    s.targetGroupPressure = targetP;
    s.targetGroupFlow = targetF;
    s.headTemp = 92.5;
    s.mixTemp = 90;
    s.frameNumber = frameNumber;
    s.steamTemp = 150;
    push(data::DataUpdate::newSampleUpdate(s));
  }

  // react to the firmware's commands the way the real devices would
  void processCommands() {
    auto q = cmdQ();
    if (!q) {
      return;
    }
    auto c = cmd::CommandRequest{cmd::CommandType::EMPTY};
    while (xQueueReceive(q, &c, 0) == pdTRUE) {
      switch (c.getType()) {
        case cmd::CommandType::SCALE_TARE:
          weight = 0;
          pushWeight();
          break;
        case cmd::CommandType::MACHINE_STOP:
          printf("[SIM] machine stop received\n");
          stopFlow();
          break;
        case cmd::CommandType::SLEEP:
          setState(MachineState::sleep, MachineSubstate::ready);
          break;
        default:
          break;
      }
    }
  }

  void tick() {
    processCommands();
    if (!de1) {
      return;
    }

    auto now = millis();
    auto t = (now - phaseStart) / 1000.0;
    bool emitDue = now - lastEmit >= 100;

    if (state == MachineState::espresso && substate == MachineSubstate::preinfusing) {
      if (emitDue) {
        emitSample(2.4 + 0.2 * sin(t * 2), 0.6, 3.0, 0.5);
      }
      if (t > 2.5) {
        setState(MachineState::espresso, MachineSubstate::pouring);
      }
    } else if (state == MachineState::espresso && substate == MachineSubstate::pouring) {
      if (emitDue) {
        double flow = 2.0 + 0.25 * sin(t * 1.3);
        emitSample(8.6 + 0.35 * sin(t), flow, 9.0, 2.0);
        frameNumber = min(7, 1 + (int)(t / 5));
        if (scale) {
          weight += flow * 0.92 * 0.1;
          pushWeight();
        }
      }
      if (t > 60) {
        stopFlow();
      }
    } else if (state == MachineState::espresso && substate == MachineSubstate::ending) {
      if (emitDue) {
        emitSample(1.0, 0.2, 0, 0);
      }
      if (t > 0.8) {
        setState(MachineState::idle, MachineSubstate::ready);
      }
    } else if (state == MachineState::hot_water) {
      if (emitDue && scale) {
        weight += 8.0 * 0.1;
        pushWeight();
      }
      if (t > 60) {
        stopFlow();
      }
    } else if (state == MachineState::hot_water_rinse) {
      if (emitDue) {
        emitSample(2.0, 4.0, 0, 0);
      }
      if (t > 3.0) {
        stopFlow();
      }
    }

    if (emitDue) {
      lastEmit = now;
    }
  }
};

static Sim sim;
static Gesture gesture;
static bool mouseTouching = false;
static bool deviceButtonHeld = false;
static char** g_argv;

// desired panel coordinates become controller coordinates, inverting the
// firmware's touch mapping (unflipped orientation)
static void setTouch(int ux, int uy) {
  emu::touchX = emu_panel_w - 1 - ux;
  emu::touchY = emu_panel_h - 1 - uy;
  emu::touchDown = true;
}

static void startSwipe(bool left) {
  gesture.active = true;
  gesture.home = false;
  gesture.frame = 0;
  gesture.frames = 10;
  gesture.fromX = left ? 420 : 116;
  gesture.toX = left ? 116 : 420;
  gesture.y = emu_panel_h / 2;
}

static void startHome() {
  gesture.active = true;
  gesture.home = true;
  gesture.frame = 0;
  gesture.frames = 5;
}

static void runGesture() {
  if (!gesture.active) {
    return;
  }
  if (gesture.frame >= gesture.frames) {
    emu::touchDown = false;
    gesture.active = false;
    return;
  }
  if (gesture.home) {
    // the round capacitive button reports out past the panel
    emu::touchX = 600;
    emu::touchY = 100;
    emu::touchDown = true;
  } else {
    int ux = gesture.fromX + (gesture.toX - gesture.fromX) * gesture.frame / (gesture.frames - 1);
    setTouch(ux, gesture.y);
  }
  gesture.frame++;
}

static void pressButton(int id) {
  switch (id) {
    case BTN_SLEEP:
      sim.setState(MachineState::sleep, MachineSubstate::ready);
      break;
    case BTN_IDLE:
      if (!sim.de1) {
        sim.connectDe1(true);
      } else {
        sim.setState(MachineState::idle, MachineSubstate::ready);
      }
      break;
    case BTN_SHOT:
    case BTN_STEAM:
    case BTN_WATER:
    case BTN_FLUSH:
    case BTN_STOP:
      sim.ghcPress(id);
      break;
    case BTN_DE1:
      sim.connectDe1(!sim.de1);
      break;
    case BTN_SCALE:
      sim.connectScale(!sim.scale);
      break;
    case BTN_TANK_DOWN:
      sim.tank = max(0, sim.tank - 5);
      sim.pushTank();
      break;
    case BTN_TANK_UP:
      sim.tank = min(40, sim.tank + 5);
      sim.pushTank();
      break;
    case BTN_SWIPE_L:
      startSwipe(true);
      break;
    case BTN_SWIPE_R:
      startSwipe(false);
      break;
    case BTN_HOME:
      startHome();
      break;
    case BTN_DEVICE:
      deviceButtonHeld = true;
      emu::setPin(COMBO_BUTTON_PIN, LOW);
      break;
  }
}

// small icons for the ghc cluster
static void iconDrop(TFT_eSprite& c, int cx, int cy, uint16_t col) {
  c.fillTriangle(cx, cy - 9, cx - 6, cy + 2, cx + 6, cy + 2, col);
  c.fillCircle(cx, cy + 3, 6, col);
}

static void iconSteam(TFT_eSprite& c, int cx, int cy, uint16_t col) {
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * 6 - 1;
    c.drawLine(x, cy + 9, x + 4, cy + 3, col);
    c.drawLine(x + 4, cy + 3, x, cy - 3, col);
    c.drawLine(x, cy - 3, x + 4, cy - 9, col);
  }
}

static void iconCup(TFT_eSprite& c, int cx, int cy, uint16_t col) {
  c.fillRoundRect(cx - 8, cy - 7, 14, 12, 3, col);
  c.drawCircle(cx + 8, cy - 1, 4, col);
  c.drawFastHLine(cx - 10, cy + 8, 20, col);
}

static void iconFlush(TFT_eSprite& c, int cx, int cy, uint16_t col) {
  c.fillRoundRect(cx - 7, cy - 9, 14, 5, 2, col);
  for (int i = -1; i <= 1; i++) {
    c.drawLine(cx + i * 4, cy - 2, cx + i * 6, cy + 8, col);
  }
}

static void iconStop(TFT_eSprite& c, int cx, int cy, uint16_t col) {
  c.fillRect(cx - 6, cy - 6, 12, 12, col);
}

static void drawGhcIcon(TFT_eSprite& c, int id, int cx, int cy, uint16_t col) {
  switch (id) {
    case BTN_WATER: iconDrop(c, cx, cy, col); break;
    case BTN_STEAM: iconSteam(c, cx, cy, col); break;
    case BTN_SHOT: iconCup(c, cx, cy, col); break;
    case BTN_FLUSH: iconFlush(c, cx, cy, col); break;
    case BTN_STOP: iconStop(c, cx, cy, col); break;
  }
}

int main(int argc, char** argv) {
  g_argv = argv;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }

  // zoom is the device window scale, 1x shows the panel pixel for pixel,
  // keys 1-4 change it live, EMU_ZOOM sets the start
  int zoom = 1;
  if (auto z = getenv("EMU_ZOOM")) {
    zoom = max(1, min(4, atoi(z)));
  }

  // nearest neighbor scaling keeps pixels crisp at every zoom
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  auto panelWin = SDL_CreateWindow("GNAT", SDL_WINDOWPOS_CENTERED, 120, dev_w * zoom, dev_h * zoom,
                                   SDL_WINDOW_ALLOW_HIGHDPI);
  auto panelRen = SDL_CreateRenderer(panelWin, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_RenderSetLogicalSize(panelRen, dev_w, dev_h);
  auto panelTex = SDL_CreateTexture(panelRen, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, dev_w, dev_h);

  auto ctrlWin = SDL_CreateWindow("GNAT Machine", SDL_WINDOWPOS_CENTERED, 120 + dev_h * zoom + 60, ctrl_w, ctrl_h,
                                  SDL_WINDOW_ALLOW_HIGHDPI);
  auto ctrlRen = SDL_CreateRenderer(ctrlWin, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_RenderSetLogicalSize(ctrlRen, ctrl_w, ctrl_h);
  auto ctrlTex = SDL_CreateTexture(ctrlRen, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, ctrl_w, ctrl_h);

  auto panelWinId = SDL_GetWindowID(panelWin);
  auto ctrlWinId = SDL_GetWindowID(ctrlWin);

  // the firmware runs on its own thread against the shims
  std::thread firmware([] {
    setup();
    loop();
  });
  firmware.detach();

  // both canvases reuse the shim's drawing code
  TFT_eSPI base(1, 1);
  TFT_eSprite panel(&base);
  panel.createSprite(dev_w, dev_h);
  TFT_eSprite ctrl(&base);
  ctrl.createSprite(ctrl_w, ctrl_h);

  bool seeded = false;
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (e.button.windowID == panelWinId) {
          int mx = e.button.x, my = e.button.y;
          if (mx >= dev_margin && mx < dev_margin + emu_panel_w && my >= dev_margin &&
              my < dev_margin + emu_panel_h) {
            if (!gesture.active) {
              mouseTouching = true;
              setTouch(mx - dev_margin, my - dev_margin);
            }
          } else {
            for (int i = 0; i < device_button_count; i++) {
              auto& b = deviceButtons[i];
              if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h) {
                pressButton(b.id);
              }
            }
          }
        } else if (e.button.windowID == ctrlWinId) {
          int mx = e.button.x, my = e.button.y;
          for (int i = 0; i < machine_button_count; i++) {
            auto& b = machineButtons[i];
            if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h) {
              pressButton(b.id);
            }
          }
          for (int i = 0; i < ghc_button_count; i++) {
            auto& g = ghcButtons[i];
            int dx = mx - (ghc_cx + g.dx), dy = my - (ghc_cy + g.dy);
            if (dx * dx + dy * dy <= (ghc_btn_r + 2) * (ghc_btn_r + 2)) {
              pressButton(g.id);
            }
          }
        }
      } else if (e.type == SDL_MOUSEMOTION && mouseTouching && e.motion.windowID == panelWinId) {
        if (e.motion.y >= dev_margin && e.motion.y < dev_margin + emu_panel_h) {
          setTouch(min(emu_panel_w - 1, max(0, e.motion.x - dev_margin)), e.motion.y - dev_margin);
        }
      } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
        if (mouseTouching) {
          mouseTouching = false;
          emu::touchDown = false;
        }
        if (deviceButtonHeld) {
          deviceButtonHeld = false;
          emu::setPin(COMBO_BUTTON_PIN, HIGH);
        }
      } else if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
        switch (e.key.keysym.sym) {
          case SDLK_z: pressButton(BTN_SLEEP); break;
          case SDLK_i: pressButton(BTN_IDLE); break;
          case SDLK_s: pressButton(BTN_SHOT); break;
          case SDLK_t: pressButton(BTN_STEAM); break;
          case SDLK_w: pressButton(BTN_WATER); break;
          case SDLK_f: pressButton(BTN_FLUSH); break;
          case SDLK_x: pressButton(BTN_STOP); break;
          case SDLK_d: pressButton(BTN_DE1); break;
          case SDLK_c: pressButton(BTN_SCALE); break;
          case SDLK_LEFTBRACKET: pressButton(BTN_TANK_DOWN); break;
          case SDLK_RIGHTBRACKET: pressButton(BTN_TANK_UP); break;
          case SDLK_LEFT: pressButton(BTN_SWIPE_L); break;
          case SDLK_RIGHT: pressButton(BTN_SWIPE_R); break;
          case SDLK_h: pressButton(BTN_HOME); break;
          case SDLK_b: pressButton(BTN_DEVICE); break;
          case SDLK_1:
          case SDLK_2:
          case SDLK_3:
          case SDLK_4:
            zoom = e.key.keysym.sym - SDLK_0;
            SDL_SetWindowSize(panelWin, dev_w * zoom, dev_h * zoom);
            break;
          case SDLK_ESCAPE: running = false; break;
        }
      } else if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_b && deviceButtonHeld) {
        deviceButtonHeld = false;
        emu::setPin(COMBO_BUTTON_PIN, HIGH);
      }
    }

    // once the firmware is up, bring the simulated kitchen online
    if (!seeded && emu::queueAt(2) && millis() > 800) {
      seeded = true;
      sim.connectDe1(true);
      sim.connectScale(true);
    }

    runGesture();
    sim.tick();

    if (emu::restartRequested) {
      printf("firmware requested restart, relaunching\n");
      execv(g_argv[0], g_argv);
    }

    // device window: the screen inset on the body with a visible border
    panel.fillSprite(dev_body_color);
    panel.drawRect(dev_margin - 2, dev_margin - 2, emu_panel_w + 4, emu_panel_h + 4, dev_border_color);
    panel.drawRect(dev_margin - 1, dev_margin - 1, emu_panel_w + 2, emu_panel_h + 2, dev_border_color);
    {
      std::lock_guard<std::mutex> lock(emu::fbMutex);
      if (emu::displayOn) {
        panel.pushImage(dev_margin, dev_margin, emu_panel_w, emu_panel_h, emu::framebuffer);
      } else {
        panel.fillRect(dev_margin, dev_margin, emu_panel_w, emu_panel_h, 0x0000);
        panel.setFreeFont(&FreeSans9pt7b);
        panel.setTextColor(0x39E7);
        panel.setTextDatum(MC_DATUM);
        panel.drawString("display off", dev_margin + emu_panel_w / 2, dev_margin + emu_panel_h / 2);
      }
    }
    panel.setFreeFont(&FreeSans9pt7b);
    panel.setTextDatum(MC_DATUM);
    for (int i = 0; i < device_button_count; i++) {
      auto& b = deviceButtons[i];
      bool lit = b.id == BTN_DEVICE && deviceButtonHeld;
      panel.fillRoundRect(b.x, b.y, b.w, b.h, 6, lit ? 0x2D46 : 0x39E7);
      panel.setTextColor(0xFFFF);
      panel.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2);
    }

    // machine window: sim controls left, ghc cluster right, status below
    ctrl.fillSprite(0x10A2);
    ctrl.setFreeFont(&FreeSans9pt7b);
    ctrl.setTextDatum(MC_DATUM);
    for (int i = 0; i < machine_button_count; i++) {
      auto& b = machineButtons[i];
      bool lit = (b.id == BTN_DE1 && sim.de1) || (b.id == BTN_SCALE && sim.scale);
      ctrl.fillRoundRect(b.x, b.y, b.w, b.h, 6, lit ? 0x2D46 : 0x39E7);
      ctrl.setTextColor(0xFFFF);
      ctrl.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2);
    }

    // the ghc pad
    ctrl.fillCircle(ghc_cx, ghc_cy, ghc_pad_r, 0x18E3);
    ctrl.drawCircle(ghc_cx, ghc_cy, ghc_pad_r, 0x4A69);
    ctrl.drawCircle(ghc_cx, ghc_cy, ghc_pad_r - 1, 0x4A69);
    for (int i = 0; i < ghc_button_count; i++) {
      auto& g = ghcButtons[i];
      bool active = sim.flowActiveFor(g.id);
      uint16_t fill = g.id == BTN_STOP ? 0x8945 : (active ? 0x2D46 : 0x31A6);
      ctrl.fillCircle(ghc_cx + g.dx, ghc_cy + g.dy, ghc_btn_r, fill);
      ctrl.drawCircle(ghc_cx + g.dx, ghc_cy + g.dy, ghc_btn_r, 0x632C);
      drawGhcIcon(ctrl, g.id, ghc_cx + g.dx, ghc_cy + g.dy, 0xFFFF);
    }

    // status, stacked to fit the narrow window
    char line1[64];
    char line2[64];
    int stateIdx = (int)sim.state;
    snprintf(line1, sizeof(line1), "%s [%d]", stateIdx <= 21 ? STATES[stateIdx] : "unknown", (int)sim.substate);
    snprintf(line2, sizeof(line2), "%0.1fg   tank %dmm", sim.weight, sim.tank);
    ctrl.fillRect(0, ctrl_h - 48, ctrl_w, 48, 0x0841);
    ctrl.setTextColor(0xBDF7);
    ctrl.setTextDatum(ML_DATUM);
    ctrl.drawString(line1, 10, ctrl_h - 35);
    ctrl.drawString(line2, 10, ctrl_h - 13);

    SDL_UpdateTexture(panelTex, nullptr, panel.getPointer(), dev_w * 2);
    SDL_RenderClear(panelRen);
    SDL_RenderCopy(panelRen, panelTex, nullptr, nullptr);
    SDL_RenderPresent(panelRen);

    SDL_UpdateTexture(ctrlTex, nullptr, ctrl.getPointer(), ctrl_w * 2);
    SDL_RenderClear(ctrlRen);
    SDL_RenderCopy(ctrlRen, ctrlTex, nullptr, nullptr);
    SDL_RenderPresent(ctrlRen);

    // EMU_SHOT=path@ms writes a ppm of the panel at the given uptime, for
    // eyeballing frames without a human at the window (EMU_SHOT_EXIT quits)
    static bool shotTaken = false;
    if (!shotTaken) {
      auto spec = getenv("EMU_SHOT");
      if (spec) {
        auto at = strchr(spec, '@');
        if (at && millis() >= (unsigned long)atol(at + 1)) {
          shotTaken = true;
          std::string path(spec, at - spec);
          auto f = fopen(path.c_str(), "wb");
          auto writePpm = [](FILE* f, const uint16_t* px, int w, int h) {
            fprintf(f, "P6\n%d %d\n255\n", w, h);
            for (int i = 0; i < w * h; i++) {
              uint16_t c = px[i];
              uint8_t rgb[3] = {uint8_t((c >> 11) << 3), uint8_t(((c >> 5) & 0x3F) << 2), uint8_t((c & 0x1F) << 3)};
              fwrite(rgb, 1, 3, f);
            }
          };
          if (f) {
            std::lock_guard<std::mutex> lock(emu::fbMutex);
            writePpm(f, emu::framebuffer, emu_panel_w, emu_panel_h);
            fclose(f);
            printf("wrote %s\n", path.c_str());
          }
          auto mf = fopen((path + ".machine.ppm").c_str(), "wb");
          if (mf) {
            writePpm(mf, ctrl.getPointer(), ctrl_w, ctrl_h);
            fclose(mf);
          }
          if (getenv("EMU_SHOT_EXIT")) {
            running = false;
          }
        }
      }
    }
  }

  SDL_Quit();
  return 0;
}
