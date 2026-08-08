// the gnat emulator host: runs the real firmware (setup/loop from
// src/main.cpp compiled against the shims) on a worker thread, renders its
// framebuffer in an SDL window, feeds mouse input in as touch, and plays
// the part of the DE1 and scale by exchanging updates and commands over the
// firmware's own queues

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

// window layout: the panel up top, controls below, scaled 2x
const int ctrl_h = 170;
const int canvas_w = emu_panel_w;
const int canvas_h = emu_panel_h + ctrl_h;

// control ids
enum {
  BTN_SLEEP,
  BTN_IDLE,
  BTN_SHOT,
  BTN_STEAM,
  BTN_WATER,
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

// three rows of controls under the panel
static Button buttons[] = {
    {8, 248, 80, 30, "Sleep", BTN_SLEEP},     {96, 248, 80, 30, "Idle", BTN_IDLE},
    {184, 248, 80, 30, "Shot", BTN_SHOT},     {272, 248, 80, 30, "Steam", BTN_STEAM},
    {360, 248, 80, 30, "Water", BTN_WATER},   {448, 248, 80, 30, "Stop", BTN_STOP},
    {8, 286, 80, 30, "DE1", BTN_DE1},         {96, 286, 80, 30, "Scale", BTN_SCALE},
    {184, 286, 80, 30, "Tank -", BTN_TANK_DOWN}, {272, 286, 80, 30, "Tank +", BTN_TANK_UP},
    {8, 324, 110, 30, "< Swipe", BTN_SWIPE_L},   {126, 324, 110, 30, "Swipe >", BTN_SWIPE_R},
    {244, 324, 110, 30, "Home", BTN_HOME},       {362, 324, 166, 30, "Button (hold)", BTN_DEVICE},
};
const int button_count = sizeof(buttons) / sizeof(buttons[0]);

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
    } else if (state == MachineState::steam || state == MachineState::hot_water) {
      setState(MachineState::idle, MachineSubstate::ready);
    }
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
      sim.startShot();
      break;
    case BTN_STEAM:
      sim.setState(MachineState::steam, MachineSubstate::steaming);
      break;
    case BTN_WATER:
      sim.setState(MachineState::hot_water, MachineSubstate::pouring);
      break;
    case BTN_STOP:
      sim.stopFlow();
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

int main(int argc, char** argv) {
  g_argv = argv;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }

  // zoom is the window scale, 1x shows the panel pixel for pixel, keys 1-4
  // change it live, EMU_ZOOM sets the start
  int zoom = 1;
  if (auto z = getenv("EMU_ZOOM")) {
    zoom = max(1, min(4, atoi(z)));
  }

  // nearest neighbor scaling keeps pixels crisp at every zoom
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  auto window = SDL_CreateWindow("GNAT Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, canvas_w * zoom,
                                 canvas_h * zoom, SDL_WINDOW_ALLOW_HIGHDPI);
  auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_RenderSetLogicalSize(renderer, canvas_w, canvas_h);
  auto texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, canvas_w, canvas_h);

  // the firmware runs on its own thread against the shims
  std::thread firmware([] {
    setup();
    loop();
  });
  firmware.detach();

  // our window canvas reuses the shim's drawing code
  TFT_eSPI base(1, 1);
  TFT_eSprite canvas(&base);
  canvas.createSprite(canvas_w, canvas_h);

  bool seeded = false;
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = false;
      } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = e.button.x, my = e.button.y;
        if (my < emu_panel_h) {
          if (!gesture.active) {
            mouseTouching = true;
            setTouch(mx, my);
          }
        } else {
          for (int i = 0; i < button_count; i++) {
            auto& b = buttons[i];
            if (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h) {
              pressButton(b.id);
            }
          }
        }
      } else if (e.type == SDL_MOUSEMOTION && mouseTouching) {
        if (e.motion.y < emu_panel_h) {
          setTouch(e.motion.x, e.motion.y);
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
            SDL_SetWindowSize(window, canvas_w * zoom, canvas_h * zoom);
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

    // compose: panel on top, controls below
    {
      std::lock_guard<std::mutex> lock(emu::fbMutex);
      if (emu::displayOn) {
        canvas.pushImage(0, 0, emu_panel_w, emu_panel_h, emu::framebuffer);
      } else {
        canvas.fillRect(0, 0, emu_panel_w, emu_panel_h, 0x0000);
        canvas.setFreeFont(&FreeSans9pt7b);
        canvas.setTextColor(0x39E7);
        canvas.setTextDatum(MC_DATUM);
        canvas.drawString("display off", emu_panel_w / 2, emu_panel_h / 2);
      }
    }

    canvas.fillRect(0, emu_panel_h, canvas_w, ctrl_h, 0x10A2);
    canvas.setFreeFont(&FreeSans9pt7b);
    canvas.setTextDatum(MC_DATUM);
    for (int i = 0; i < button_count; i++) {
      auto& b = buttons[i];
      bool lit = (b.id == BTN_DE1 && sim.de1) || (b.id == BTN_SCALE && sim.scale) ||
                 (b.id == BTN_DEVICE && deviceButtonHeld);
      canvas.fillRoundRect(b.x, b.y, b.w, b.h, 6, lit ? 0x2D46 : 0x39E7);
      canvas.setTextColor(0xFFFF);
      canvas.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2);
    }

    // status line
    char status[128];
    int stateIdx = (int)sim.state;
    snprintf(status, sizeof(status), "%s [%d]   %0.1fg   tank %dmm", stateIdx <= 21 ? STATES[stateIdx] : "unknown",
             (int)sim.substate, sim.weight, sim.tank);
    canvas.fillRect(0, canvas_h - 26, canvas_w, 26, 0x0841);
    canvas.setTextColor(0xBDF7);
    canvas.setTextDatum(ML_DATUM);
    canvas.drawString(status, 10, canvas_h - 13);

    SDL_UpdateTexture(texture, nullptr, canvas.getPointer(), canvas_w * 2);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    // EMU_SHOT=path@ms writes a ppm of the canvas at the given uptime, for
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
          if (f) {
            fprintf(f, "P6\n%d %d\n255\n", canvas_w, canvas_h);
            auto px = canvas.getPointer();
            for (int i = 0; i < canvas_w * canvas_h; i++) {
              uint16_t c = px[i];
              uint8_t rgb[3] = {uint8_t((c >> 11) << 3), uint8_t(((c >> 5) & 0x3F) << 2), uint8_t((c & 0x1F) << 3)};
              fwrite(rgb, 1, 3, f);
            }
            fclose(f);
            printf("wrote %s\n", path.c_str());
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
