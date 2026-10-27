# pocket-tank PC simulator (LVGL v9 + SDL2)
#
# macOS: built as x86_64 by default because the Homebrew SDL2 here is the
# Intel build (/usr/local); the sim runs fine under Rosetta. `make ARCH=` for
# a native build against an arm64 SDL2. Elsewhere (Linux, CI) ARCH is empty
# and the host compiler's own architecture is used.

CC      := cc
ifeq ($(shell uname -s),Darwin)
ARCH    ?= -arch x86_64
endif
ARCH    ?=
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

# `make ROUND=1`: the round 466 px bowl (the Waveshare 1.75C's world) as
# ./fishsim-round, built beside the rectangle's in build-round/.
# `make WATCH=1`: the watch's 410 x 502 portrait tank (the Waveshare 2.06) as ./fishsim-watch
# `make SQUARE=1`: the square's 480 x 480 tank (the Waveshare 2.16) as ./fishsim-square
# `make LILI=1`: Pixel Lili's own simulator (lili_main.c + common/lili) as ./lilisim;
# the fish targets below are untouched. `make LILI=1 check` runs its selftests.
ifdef LILI
BUILD   := build-lili
OUT     := lilisim
ROUNDF  := -DLILI_SIM
else ifdef ROUND
BUILD   := build-round
OUT     := fishsim-round
ROUNDF  := -DTANK_ROUND
else ifdef WATCH
BUILD   := build-watch
OUT     := fishsim-watch
ROUNDF  := -DTANK_WATCH
else ifdef SQUARE
BUILD   := build-square
OUT     := fishsim-square
ROUNDF  := -DTANK_SQUARE
else
BUILD   := build
OUT     := fishsim
ROUNDF  :=
endif

CFLAGS  := $(ROUNDF) -O2 -std=c11 $(ARCH) -I. -I../common -I../common/llm -DLV_CONF_INCLUDE_SIMPLE -DSOUNDS_BIN='"../assets/sounds/sounds.bin"' -DPT_VERSION='"$(shell git describe --always --dirty --exclude='*' 2>/dev/null || echo sim)"' $(SDL_CFLAGS) \
           -Wall -Wextra -Wno-unused-parameter -MMD -MP
LDFLAGS := $(ARCH) $(SDL_LIBS) -lm

ifdef LILI
APP_SRCS  := lili_main.c inspector.c ../common/lili/events.c ../common/lili/lili_world.c ../common/lili/lili_demo.c ../common/lili/nav.c ../common/lili/gesture.c ../common/lili/plan.c ../common/lili/toys.c ../common/lili/schema_gen.c ../common/lili/life.c ../common/lili/save.c ../common/lili/render8.c ../common/lili/palette.c ../common/lili/palette_gen.c ../common/lili/pak.c ../common/lili/mask.c ../common/lili/maps_gen.c
CFLAGS    += -I../common/lili
else
APP_SRCS  := main.c advisor_llm.c persist_port_sim.c net_port_sim.c ../common/tank.c ../common/update.c ../common/advisor.c ../common/render.c ../common/icons.c ../common/progression.c ../common/setup.c ../common/audio.c ../common/notice.c ../common/sounds.c ../common/battery.c ../common/llm/q4_model.c ../common/llm/word_tok.c ../common/llm/advisor_core.c
endif

LVGL_SRCS := $(shell find lvgl/src -name '*.c')
OBJS      := $(patsubst %.c,$(BUILD)/%.o,$(filter-out ../common/%,$(APP_SRCS)) $(LVGL_SRCS)) $(patsubst ../common/%.c,$(BUILD)/common/%.o,$(filter ../common/%,$(APP_SRCS)))

$(OUT): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

SELFTESTS := --selftest --selftest-hunger --selftest-shop --selftest-battery \
             --selftest-pop --selftest-card --selftest-fry --selftest-sleep --selftest-tend --selftest-llm \
             --selftest-saves --selftest-update
# the dev repo does not track model/out (the public one does): NOMODEL=1 runs
# the same checks without the LLM selftest, for a checkout with no model
ifdef LILI
SELFTESTS := --selftest --selftest-nav --selftest-render --selftest-gesture --selftest-plan --selftest-reflex --selftest-toys --selftest-progress --selftest-calendar --selftest-offline --selftest-save --bench
endif
ifdef NOMODEL
SELFTESTS := $(filter-out --selftest-llm,$(SELFTESTS))
endif

# `make check` tests the rectangle; `make ROUND=1 check` / `make WATCH=1 check` /
# `make SQUARE=1 check` the same selftests in that board's world; `make check-all`
# all four - the gate for anything in common/ (docs/BOARDS.md)
check: $(OUT)
	@set -e; for test in $(SELFTESTS); do echo "RUN $(OUT) $$test"; ./$(OUT) "$$test"; done

check-all:
	$(MAKE) check
	$(MAKE) ROUND=1 check
	$(MAKE) WATCH=1 check
	$(MAKE) SQUARE=1 check

check-nomodel:
	$(MAKE) NOMODEL=1 check

$(BUILD)/common/llm/%.o: ../common/llm/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -O3 -c $< -o $@

$(BUILD)/common/%.o: ../common/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build build-round build-watch build-square build-lili fishsim fishsim-round fishsim-watch fishsim-square lilisim

# LVGL is pinned (v9.2.2) and never edited: the Lili target tracks only its own
# headers. Reading ~300 LVGL .d files means thousands of header checks, and on
# /mnt/c (the repo's home, a network-like 9P mount) that took minutes per make.
ifdef LILI
# the maps are data: edit assets/maps/*.json, this regenerates the C (never edit maps_gen.*)
../common/lili/maps_gen.c ../common/lili/maps_gen.h: ../tools/gen_maps.py ../common/lili/config.h $(wildcard ../assets/maps/*.json)
	python3 ../tools/gen_maps.py
$(BUILD)/common/lili/lili_world.o $(BUILD)/common/lili/lili_demo.o $(BUILD)/common/lili/nav.o $(BUILD)/lili_main.o: ../common/lili/maps_gen.h
-include $(filter-out $(BUILD)/lvgl/%,$(OBJS:.o=.d))
else
-include $(OBJS:.o=.d)
endif

.PHONY: check check-all check-nomodel clean
