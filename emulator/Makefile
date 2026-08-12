# builds the gnat emulator: the real firmware compiled against desktop
# shims plus an SDL host. requires sdl2 (brew install sdl2).

CXX ?= c++
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)

DEFINES = -DDISPLAY_RM67162 -DTOUCH_CST816 -DCOMBO_BUTTON_PIN=0 -DMENU_BUTTON_PIN=0 \
          -DUI_SCALE_PCT=178 -DTFT_WIDTH=240 -DTFT_HEIGHT=536 -DBUILD_VERSION= \
          -DTOUCH_SDA_PIN=1 -DTOUCH_SCL_PIN=2

CXXFLAGS = -std=c++17 -O2 -g -Wall -Wno-unused-variable -Wno-sign-compare \
           -Ishim -I../src $(SDL_CFLAGS) $(DEFINES)

OBJS = build/main_fw.o build/emu_bridge.o build/main_host.o

gnat-emu: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(SDL_LIBS)

build/main_fw.o: ../src/main.cpp Makefile $(wildcard shim/*.h shim/*/*.h) | build
	$(CXX) $(CXXFLAGS) -c ../src/main.cpp -o $@

build/emu_bridge.o: shim/emu_bridge.cpp Makefile $(wildcard shim/*.h) | build
	$(CXX) $(CXXFLAGS) -c shim/emu_bridge.cpp -o $@

build/main_host.o: host/main_host.cpp Makefile $(wildcard shim/*.h shim/*/*.h) | build
	$(CXX) $(CXXFLAGS) -c host/main_host.cpp -o $@

build:
	mkdir -p build

# tests. the driver test compiles the real rm67162 driver against a fake SPI
# bus (no SDL) and checks the wake path re-lights the panel; the wake test
# runs the real firmware loop headless against the shims to confirm the
# sleep/wake logic recovers the display across screens. neither needs sdl2, so
# `make test` works without it.
TEST_FLAGS = -std=c++17 -O0 -g -Wall -Wno-unused-variable -Wno-sign-compare $(DEFINES)

build/rm67162_wake_test: test/rm67162_wake_test.cpp ../src/amoled/rm67162.cpp Makefile \
                         $(wildcard test/fakes/*.h test/fakes/*/*.h) | build
	$(CXX) $(TEST_FLAGS) -DLCD_USB_QSPI_DREVER=1 -Itest/fakes -I../src/amoled \
	    test/rm67162_wake_test.cpp ../src/amoled/rm67162.cpp -o $@

build/wake_test: test/wake_test.cpp ../src/main.cpp shim/emu_bridge.cpp Makefile \
                 $(wildcard shim/*.h shim/*/*.h) | build
	$(CXX) $(TEST_FLAGS) -Ishim -I../src \
	    test/wake_test.cpp ../src/main.cpp shim/emu_bridge.cpp -o $@

build/display_controller_test: test/display_controller_test.cpp ../src/controller/DisplayController.h Makefile | build
	$(CXX) $(TEST_FLAGS) -I../src test/display_controller_test.cpp -o $@

test: build/rm67162_wake_test build/display_controller_test build/wake_test
	@echo "== rm67162 driver wake =="; ./build/rm67162_wake_test
	@echo "== display controller sequencing =="; ./build/display_controller_test
	@echo "== firmware loop sleep/wake ==";  ./build/wake_test >/dev/null && echo "PASSED" || (./build/wake_test; false)

clean:
	rm -rf build gnat-emu

.PHONY: clean test
