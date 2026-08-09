// the gnat emulator host: runs the real firmware (setup/loop from
// src/main.cpp compiled against the shims) on a worker thread and shows it
// in a single window: the screen inset on a light gray device body with the
// device's own inputs below it, and the simulated machine in a dark column
// on the right, headlined by a ghc cluster mirroring the real controls. the
// mouse feeds the screen as touch.

#include <Arduino.h>

#include <Config.h>
#include <Data.h>
#include <Command.h>

#include <SDL.h>
#include <TFT_eSPI.h>
#include <unistd.h>

#include <thread>

#include "../shim/emu_bridge.h"
#include "shot_data.h"

extern void setup();
extern void loop();

// the screen sits inset on a light gray body with a border so its
// boundaries are obvious, device inputs in a strip below it
const int dev_margin = 16;
const int strip_h = 33;
const int strip_y = dev_margin + emu_panel_h + dev_margin;
const uint16_t dev_body_color = 0xD69A;
const uint16_t dev_border_color = 0x632C;

// the machine column on the right keeps its dark background, top aligned
// with the screen and bottom aligned with the device buttons
const int machine_gap = 12;
const int machine_w = 180;
const int machine_x = dev_margin + emu_panel_w + machine_gap;
const int machine_y = dev_margin;
const int machine_h = strip_y + strip_h - dev_margin;
const uint16_t machine_bg_color = 0x10A2;

const int win_w = machine_x + machine_w + dev_margin;
const int win_h = strip_y + strip_h + dev_margin;

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

// device strip: only inputs the physical device has
static Button deviceButtons[] = {
    {16, strip_y, 120, 28, "Btn (hold)", BTN_DEVICE},
    {150, strip_y, 100, 28, "Home", BTN_HOME},
    {286, strip_y, 120, 28, "< Swipe", BTN_SWIPE_L},
    {418, strip_y, 120, 28, "Swipe >", BTN_SWIPE_R},
};
const int device_button_count = sizeof(deviceButtons) / sizeof(deviceButtons[0]);

// machine column, coordinates local to the column
static Button machineButtons[] = {
    {8, 172, 80, 27, "Sleep", BTN_SLEEP},      {92, 172, 80, 27, "Idle", BTN_IDLE},
    {8, 203, 80, 27, "DE1", BTN_DE1},          {92, 203, 80, 27, "Scale", BTN_SCALE},
    {8, 234, 80, 27, "Tank -", BTN_TANK_DOWN}, {92, 234, 80, 27, "Tank +", BTN_TANK_UP},
};
const int machine_button_count = sizeof(machineButtons) / sizeof(machineButtons[0]);

// the ghc cluster mirroring the real machine: hot water up top, steam on
// the right, espresso at the bottom, flush on the left, stop in the middle
const int ghc_cx = machine_w / 2;
const int ghc_cy = 86;
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

  // shot playback follows a recorded visualizer shot, dripping a little
  // extra into the cup after a stop
  unsigned long shotStart = 0;
  int playIndex = 0;
  double drip = 0;

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
    playIndex = 0;
    drip = 0;
    shotStart = millis();
    setState(MachineState::espresso, MachineSubstate::preinfusing);
  }

  void stopFlow() {
    if (state == MachineState::espresso) {
      drip = 1.2;
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

  void emitSample(double pressure, double flow, double targetP, double targetF, double headTemp = 92.5) {
    data::Sample s{};
    sampleTime += 100;
    s.sampleTime = sampleTime;
    s.groupPressure = pressure;
    s.groupFlow = flow;
    s.targetGroupPressure = targetP;
    s.targetGroupFlow = targetF;
    s.headTemp = headTemp;
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

    if (state == MachineState::espresso &&
        (substate == MachineSubstate::preinfusing || substate == MachineSubstate::pouring)) {
      // play the recorded shot at its own pace
      auto st = (now - shotStart) / 1000.0;
      while (playIndex < shot_sample_count && shot_time[playIndex] <= st) {
        auto i = playIndex++;
        frameNumber = shot_frame[i];
        emitSample(shot_pressure[i], shot_flow[i], shot_pressure_goal[i], shot_flow_goal[i], shot_temp[i]);
        if (scale) {
          weight = shot_weight[i];
          pushWeight();
        }
        if (i == shot_pour_start_index && substate == MachineSubstate::preinfusing) {
          setState(MachineState::espresso, MachineSubstate::pouring);
        }
      }
      if (playIndex >= shot_sample_count) {
        stopFlow();
      }
    } else if (state == MachineState::espresso && substate == MachineSubstate::ending) {
      if (emitDue) {
        emitSample(1.0, 0.2, 0, 0);
        // the last of the pour drips in
        if (scale && drip > 0) {
          auto d = min(drip, 0.15);
          weight += d;
          drip -= d;
          pushWeight();
        }
      }
      if (t > 0.8 && drip <= 0.01) {
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

  // zoom is the window scale, 1x shows the screen pixel for pixel, keys
  // 1-4 change it live, EMU_ZOOM sets the start
  int zoom = 1;
  if (auto z = getenv("EMU_ZOOM")) {
    zoom = max(1, min(4, atoi(z)));
  }

  // nearest neighbor scaling keeps pixels crisp at every zoom
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  auto window = SDL_CreateWindow("GNAT Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w * zoom,
                                 win_h * zoom, SDL_WINDOW_ALLOW_HIGHDPI);
  auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_RenderSetLogicalSize(renderer, win_w, win_h);
  auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, win_w, win_h);

  // the firmware runs on its own thread against the shims
  std::thread firmware([] {
    setup();
    loop();
  });
  firmware.detach();

  // the window canvas reuses the shim's drawing code
  TFT_eSPI base(1, 1);
  TFT_eSprite canvas(&base);
  canvas.createSprite(win_w, win_h);

  bool seeded = false;
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = e.button.x, my = e.button.y;
        if (mx >= dev_margin && mx < dev_margin + emu_panel_w && my >= dev_margin && my < dev_margin + emu_panel_h) {
          if (!gesture.active) {
            mouseTouching = true;
            setTouch(mx - dev_margin, my - dev_margin);
          }
        } else if (mx >= machine_x) {
          int lx = mx - machine_x, ly = my - machine_y;
          for (int i = 0; i < machine_button_count; i++) {
            auto& b = machineButtons[i];
            if (lx >= b.x && lx <= b.x + b.w && ly >= b.y && ly <= b.y + b.h) {
              pressButton(b.id);
            }
          }
          for (int i = 0; i < ghc_button_count; i++) {
            auto& g = ghcButtons[i];
            int dx = lx - (ghc_cx + g.dx), dy = ly - (ghc_cy + g.dy);
            if (dx * dx + dy * dy <= (ghc_btn_r + 2) * (ghc_btn_r + 2)) {
              pressButton(g.id);
            }
          }
        } else {
          for (int i = 0; i < device_button_count; i++) {
            auto& b = deviceButtons[i];
            if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h) {
              pressButton(b.id);
            }
          }
        }
      } else if (e.type == SDL_MOUSEMOTION && mouseTouching) {
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
            SDL_SetWindowSize(window, win_w * zoom, win_h * zoom);
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

    // EMU_AUTOSHOT starts the recorded shot on its own, for headless runs
    static bool autoShot = false;
    if (!autoShot && seeded && getenv("EMU_AUTOSHOT") && millis() > 2500) {
      autoShot = true;
      sim.startShot();
    }

    runGesture();
    sim.tick();

    if (emu::restartRequested) {
      printf("firmware requested restart, relaunching\n");
      execv(g_argv[0], g_argv);
    }

    // the device body with the screen inset behind a border
    canvas.fillSprite(dev_body_color);
    canvas.drawRect(dev_margin - 2, dev_margin - 2, emu_panel_w + 4, emu_panel_h + 4, dev_border_color);
    canvas.drawRect(dev_margin - 1, dev_margin - 1, emu_panel_w + 2, emu_panel_h + 2, dev_border_color);
    {
      std::lock_guard<std::mutex> lock(emu::fbMutex);
      if (emu::displayOn) {
        canvas.pushImage(dev_margin, dev_margin, emu_panel_w, emu_panel_h, emu::framebuffer);
      } else {
        canvas.fillRect(dev_margin, dev_margin, emu_panel_w, emu_panel_h, 0x0000);
        canvas.setFreeFont(&FreeSans9pt7b);
        canvas.setTextColor(0x39E7);
        canvas.setTextDatum(MC_DATUM);
        canvas.drawString("display off", dev_margin + emu_panel_w / 2, dev_margin + emu_panel_h / 2);
      }
    }

    canvas.setFreeFont(&FreeSans9pt7b);
    canvas.setTextDatum(MC_DATUM);
    for (int i = 0; i < device_button_count; i++) {
      auto& b = deviceButtons[i];
      bool lit = b.id == BTN_DEVICE && deviceButtonHeld;
      canvas.fillRoundRect(b.x, b.y, b.w, b.h, 6, lit ? 0x2D46 : 0x632C);
      canvas.setTextColor(0xFFFF);
      canvas.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2);
    }

    // the machine column keeps its dark background
    canvas.fillRoundRect(machine_x, machine_y, machine_w, machine_h, 8, machine_bg_color);
    for (int i = 0; i < machine_button_count; i++) {
      auto& b = machineButtons[i];
      bool lit = (b.id == BTN_DE1 && sim.de1) || (b.id == BTN_SCALE && sim.scale);
      canvas.fillRoundRect(machine_x + b.x, machine_y + b.y, b.w, b.h, 6, lit ? 0x2D46 : 0x39E7);
      canvas.setTextColor(0xFFFF);
      canvas.drawString(b.label, machine_x + b.x + b.w / 2, machine_y + b.y + b.h / 2);
    }

    // the ghc pad
    int gx = machine_x + ghc_cx, gy = machine_y + ghc_cy;
    canvas.fillCircle(gx, gy, ghc_pad_r, 0x18E3);
    canvas.drawCircle(gx, gy, ghc_pad_r, 0x4A69);
    canvas.drawCircle(gx, gy, ghc_pad_r - 1, 0x4A69);
    for (int i = 0; i < ghc_button_count; i++) {
      auto& g = ghcButtons[i];
      bool active = sim.flowActiveFor(g.id);
      uint16_t fill = g.id == BTN_STOP ? 0x8945 : (active ? 0x2D46 : 0x31A6);
      canvas.fillCircle(gx + g.dx, gy + g.dy, ghc_btn_r, fill);
      canvas.drawCircle(gx + g.dx, gy + g.dy, ghc_btn_r, 0x632C);
      drawGhcIcon(canvas, g.id, gx + g.dx, gy + g.dy, 0xFFFF);
    }

    // compact status at the bottom of the machine column
    char status[64];
    int stateIdx = (int)sim.state;
    snprintf(status, sizeof(status), "%s  %0.0fg  %dmm", stateIdx <= 21 ? STATES[stateIdx] : "unknown", sim.weight,
             sim.tank);
    canvas.setTextColor(0xBDF7);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString(status, machine_x + machine_w / 2, machine_y + machine_h - 16);

    SDL_UpdateTexture(texture, nullptr, canvas.getPointer(), win_w * 2);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    // EMU_SHOT=path@ms writes the raw screen as a ppm at the given uptime,
    // with the full window alongside (EMU_SHOT_EXIT quits after)
    static bool shotTaken = false;
    if (!shotTaken) {
      auto spec = getenv("EMU_SHOT");
      if (spec) {
        auto at = strchr(spec, '@');
        if (at && millis() >= (unsigned long)atol(at + 1)) {
          shotTaken = true;
          std::string path(spec, at - spec);
          auto writePpm = [](FILE* f, const uint16_t* px, int w, int h) {
            fprintf(f, "P6\n%d %d\n255\n", w, h);
            for (int i = 0; i < w * h; i++) {
              uint16_t c = px[i];
              uint8_t rgb[3] = {uint8_t((c >> 11) << 3), uint8_t(((c >> 5) & 0x3F) << 2), uint8_t((c & 0x1F) << 3)};
              fwrite(rgb, 1, 3, f);
            }
          };
          auto f = fopen(path.c_str(), "wb");
          if (f) {
            std::lock_guard<std::mutex> lock(emu::fbMutex);
            writePpm(f, emu::framebuffer, emu_panel_w, emu_panel_h);
            fclose(f);
            printf("wrote %s\n", path.c_str());
          }
          auto wf = fopen((path + ".window.ppm").c_str(), "wb");
          if (wf) {
            writePpm(wf, canvas.getPointer(), win_w, win_h);
            fclose(wf);
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
