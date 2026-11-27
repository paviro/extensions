# SPDX-License-Identifier: GPL-3.0-only
main_SOURCES := main.c look.c style.c render.c catalog.c crc.c tint.c selection_bar.c \
    menu_covers.c flat_cover.c latch.c theme.c theme_picker.c lz.c assets.c dark_mode.c \
    ../shared/schedule.c ../shared/sun.c
switch_SOURCES := switch.c pack.c derive.c stock.c
OUT ?= build
# The theme compiler and the Classic theme live in themes/ at the repository
# root: rvt.h and the bitmap catalog come from there, and the Classic dark
# artwork is compiled from there into build/classic.rvt, which stays under
# build/ whatever OUT is, where manifest.toml names it.
THEMES ?= ../../themes
RVT_DATA := $(THEMES)/rvt/data
CATALOG := $(RVT_DATA)/osos-2.0.5.json
CLASSIC := build/classic.rvt
EXTRA_CFLAGS := -I$(OUT) -I../shared -I$(RVT_DATA)
include $(SDK)/rv.mk

$(OUT)/catalog.inc: $(CATALOG) tools/catalog_inc.py | $(OUT)
	$(PYTHON) tools/catalog_inc.py $< $@
$(OUT)/main/catalog.c.o: $(OUT)/catalog.inc

$(CLASSIC): $(THEMES)/themes/classic/theme.toml \
    $(wildcard $(THEMES)/themes/classic/dark/apple/*) $(wildcard $(THEMES)/rvt/*.py) \
    $(wildcard $(RVT_DATA)/*) tools/catalog_inc.py
	mkdir -p build
	cd $(THEMES) && $(PYTHON) -m rvt compile themes/classic --allow-reserved --strict \
	    --out $(abspath build)/classic-build >/dev/null
	$(PYTHON) tools/catalog_inc.py $(CATALOG) --check build/classic-build/classic/classic.rvt
	cp build/classic-build/classic/classic.rvt $@
