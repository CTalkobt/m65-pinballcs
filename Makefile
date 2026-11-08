# Pinball Construction Set for the MEGA65
#
#   make              build/pinball.d81: PINBALL (the menu and the editor), PLAY
#                     (the game), the art and sounds, the 12 tables
#   make run          boot the disk in xemu-xmega65
#   make screenshot   run it headless, save build/screenshot.png (SHOT_DELAY)
#   make previews     every table drawn on the PC: build/previews/sheet.png
#   make sim          the robot plays every table on the PC
#   make clean
#
# Variables: M65 (the M65Compiler tree, default ../m65compiler), XEMU,
# SHOT_DELAY (seconds), DEFS.

M65     ?= ../m65compiler
# CC45_NO_ZP_PARAMS=1: parameters always copied at a function's entry. cc45
# keeps some in the ZP parameter block when they are "used only before the
# first call"; it misjudged that for render.c's part functions (a pointer
# parameter dereferenced after a call that had overwritten $10/$11).
CC45    := CC45_NO_ZP_PARAMS=1 $(M65)/bin/cc45
CA45    := $(M65)/bin/ca45
LN45    := $(M65)/bin/ln45
DISK45  := $(M65)/bin/disk45
TUNE45  := $(M65)/bin/tune45
STDLIB  := $(M65)/lib/build/c45.lib
XEMU    ?= xemu-xmega65
PYTHON  ?= python3
HOSTCC  ?= cc

BUILD   ?= build
DEFS    ?=
CFLAGS  := -O2 -ffunction-sections -I$(M65)/lib/include -Isrc -I$(BUILD) $(DEFS)
# both programs: the RAM at $C000-$CFFF is theirs (disk.s45 maps the ROM in
# only for KERNAL calls), the stack small
LNFLAGS := -basic --gc-inputs --ram-top=0xD000 --stack-size=0x200 --overlay-map --overlay-store=0x30000-0x3BFFF,0x24000-0x27FFF

SHARED  := table geom theme video sound music files world physics isqrt
PB_MODS := pinball ui editor edpanel prep render rprim rbg rpoly physbuild
PL_MODS := main_play play game robot contact
PB_OBJS := $(PB_MODS:%=$(BUILD)/%.o45) $(SHARED:%=$(BUILD)/%.o45) $(BUILD)/gen_tables.o45 $(BUILD)/disk.o45 $(BUILD)/fmath.o45 $(BUILD)/blob.o45 $(BUILD)/menu_tunes.o45
PL_OBJS := $(PL_MODS:%=$(BUILD)/%.o45) $(SHARED:%=$(BUILD)/%.o45) $(BUILD)/gen_tables.o45 $(BUILD)/disk.o45 $(BUILD)/fmath.o45 $(BUILD)/game_tunes.o45
HEADERS := $(wildcard src/*.h) $(BUILD)/assets.h $(BUILD)/menu_tunes.h $(BUILD)/game_tunes.h $(BUILD)/gen_render.h
PINBALL := $(BUILD)/pinball.prg
PLAY    := $(BUILD)/play.prg
DISK    := $(BUILD)/pinball.d81
HOST_CORE := src/table.c src/geom.c src/isqrt.c src/theme.c src/render.c src/rprim.c src/rbg.c src/rpoly.c src/physics.c src/contact.c src/physbuild.c src/world_host.c $(BUILD)/gen_tables.c

.PHONY: all run screenshot previews sim clean

all: $(DISK)

$(BUILD):
	mkdir -p $@

$(BUILD)/gen_tables.c $(BUILD)/gen_render.h &: tools/gen_tables.py | $(BUILD)
	$(PYTHON) tools/gen_tables.py $(BUILD)

$(BUILD)/assets.h $(BUILD)/pbgfx.bin &: tools/gen_assets.py | $(BUILD)
	$(PYTHON) tools/gen_assets.py $(BUILD)

$(BUILD)/tables.list: tools/tables.py tools/tablelib.py $(BUILD)/gen_tables.c | $(BUILD)
	$(PYTHON) tools/tables.py $(BUILD)

$(BUILD)/%_tunes.c $(BUILD)/%_tunes.h: assets/%.txt | $(BUILD)
	$(TUNE45) convert $< -o $(BUILD)/$*_tunes.c

$(BUILD)/%.o45: src/%.c $(HEADERS) | $(BUILD)
	$(CC45) $(CFLAGS) -c $< -o $@

# the stack at the top of the RAM at $C000-$CFFF (each program's main)
$(BUILD)/main_play.o45 $(BUILD)/pinball.o45: CFLAGS += -fstack-top=0xCFFF
# the code overlays (mapped: kept in the ROM area, shown at $6000-$7FFF;
# 8 KB at most each)
$(BUILD)/ui.o45 $(BUILD)/files.o45 $(BUILD)/table.o45: CFLAGS += -foverlay=menu
$(BUILD)/editor.o45: CFLAGS += -foverlay=edit
$(BUILD)/edpanel.o45: CFLAGS += -foverlay=edp
$(BUILD)/render.o45: CFLAGS += -foverlay=draw1
$(BUILD)/rprim.o45: CFLAGS += -foverlay=draw2
$(BUILD)/rbg.o45: CFLAGS += -foverlay=draw3
$(BUILD)/rpoly.o45: CFLAGS += -foverlay=draw4
$(BUILD)/prep.o45 $(BUILD)/physbuild.o45 $(BUILD)/world.o45: CFLAGS += -foverlay=prep
$(BUILD)/geom.o45: CFLAGS += -foverlay=geo
$(BUILD)/video.o45: CFLAGS += -foverlay=vid
$(BUILD)/theme.o45: CFLAGS += -foverlay=thm
$(BUILD)/sound.o45: CFLAGS += -foverlay=snd
$(BUILD)/game.o45 $(BUILD)/robot.o45: CFLAGS += -foverlay=rules
$(BUILD)/physics.o45: CFLAGS += -foverlay=phys
$(BUILD)/contact.o45: CFLAGS += -foverlay=cont
$(BUILD)/play.o45: CFLAGS += -foverlay=play

$(BUILD)/gen_tables.o45: $(BUILD)/gen_tables.c
	$(CC45) $(CFLAGS) -c $< -o $@

$(BUILD)/%_tunes.o45: $(BUILD)/%_tunes.c $(BUILD)/%_tunes.h
	$(CC45) $(CFLAGS) -c $< -o $@

$(BUILD)/disk.o45: src/disk.s45 | $(BUILD)
	$(CA45) -c $< -o $@

$(PINBALL) $(BUILD)/pinball.ov1 $(BUILD)/pinball.ov2 &: $(PB_OBJS) $(STDLIB)
	$(LN45) $(LNFLAGS) -M $(BUILD)/pinball.map -o $(PINBALL) $(PB_OBJS) $(STDLIB)

$(PLAY) $(BUILD)/play.ov1 &: $(PL_OBJS) $(STDLIB)
	$(LN45) $(LNFLAGS) -M $(BUILD)/play.map -o $(PLAY) $(PL_OBJS) $(STDLIB)

$(STDLIB):
	$(MAKE) -C $(M65) lib

$(DISK): $(PINBALL) $(PLAY) $(BUILD)/pbgfx.bin $(BUILD)/tables.list
	rm -f $@
	$(DISK45) create $@ -n "PINBALL CS" -i "PB"
	$(DISK45) add $@ $(PINBALL) PINBALL
	$(DISK45) add $@ $(BUILD)/pinball.ov1 PINBALL.OV1
	$(DISK45) add $@ $(BUILD)/pinball.ov2 PINBALL.OV2
	$(DISK45) add $@ $(PLAY) PLAY
	$(DISK45) add $@ $(BUILD)/play.ov1 PLAY.OV1
	$(DISK45) add $@ $(BUILD)/pbgfx.bin PBGFX
	$(DISK45) add $@ $(BUILD)/pbindex.prg PBINDEX
	for t in $$(cat $(BUILD)/tables.list); do $(DISK45) add $@ $(BUILD)/$$t.prg $$(echo $$t | tr a-z A-Z); done

run: $(DISK)
	$(XEMU) -8 $(abspath $(DISK)) -autoload

screenshot: $(DISK)
	AUTOLOAD=1 $(PYTHON) tools/screenshot.py $(PINBALL) $(BUILD)/screenshot.png "$(SHOT_DELAY)" $(DISK)

$(BUILD)/preview: host/preview.c $(HOST_CORE) $(wildcard src/*.h) $(BUILD)/gen_render.h | $(BUILD)
	$(HOSTCC) -O2 -Isrc -I$(BUILD) -o $@ host/preview.c $(HOST_CORE)

$(BUILD)/sim: host/sim.c $(HOST_CORE) $(wildcard src/*.h) $(BUILD)/gen_render.h | $(BUILD)
	$(HOSTCC) -O2 -Isrc -I$(BUILD) -o $@ host/sim.c $(HOST_CORE)

previews: $(BUILD)/preview $(BUILD)/tables.list
	mkdir -p $(BUILD)/previews
	for t in $$(cat $(BUILD)/tables.list); do $(BUILD)/preview $(BUILD)/$$t.tbl $(BUILD)/previews/$$t.ppm; done
	$(PYTHON) tools/sheet.py $(BUILD)/previews

sim: $(BUILD)/sim $(BUILD)/tables.list
	for t in $$(tail -n +2 $(BUILD)/tables.list); do $(BUILD)/sim $(BUILD)/$$t.tbl 100000 | grep -v "^  "; done

clean:
	rm -rf $(BUILD)

$(BUILD)/fmath.o45: src/fmath.s45 | $(BUILD)
	$(CA45) -c $< -o $@

$(BUILD)/blob.o45: src/blob.s45 | $(BUILD)
	$(CA45) -c $< -o $@
