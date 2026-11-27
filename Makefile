# SPDX-License-Identifier: GPL-3.0-only
SDK ?= ../osos-lab/rvx-sdk
OUT ?= build
extensions := album-artists appearance balance clicker-modes crossfeed dsp equalizer \
              eu-volume-limit file-info homebrew iap-auth-bypass night-shift \
              playback-log playback-queue sound-menu

.PHONY: all $(extensions)
all: $(extensions) $(OUT)/assets.json

$(extensions):
	$(MAKE) -C extensions/$@ all SDK="$(abspath $(SDK))" OUT="$(abspath $(OUT))/.work/$@"
	@mkdir -p "$(OUT)/Extensions"
	cp "$(OUT)/.work/$@/$@.rvx" "$(OUT)/Extensions/$@.rvx"

# The bitmap assets of the core and every extension, for the theme tooling
# (themes/docs/catalog.md).
$(OUT)/assets.json: $(extensions)
	cat "$(abspath $(SDK))/assets.json" > $@
	@for extension in $(extensions); do \
	    $(PYTHON) "$(abspath $(SDK))/assets.py" extensions/$$extension/manifest.toml \
	        "$(OUT)/Extensions/$$extension.rvx" >> $@ || exit 1; \
	done

.PHONY: check
check:
	$(PYTHON) extensions/album-artists/tests/run.py "$(abspath $(SDK))"
	$(PYTHON) extensions/appearance/tests/run.py "$(abspath $(SDK))"
	$(PYTHON) extensions/balance/tests/run.py "$(abspath $(SDK))"
	$(PYTHON) extensions/crossfeed/tests/run.py "$(abspath $(SDK))"
	$(PYTHON) extensions/dsp/tests/run.py "$(abspath $(SDK))"
	$(PYTHON) extensions/equalizer/tests/run.py "$(abspath $(SDK))"
	$(PYTHON) extensions/file-info/tests/run.py "$(abspath $(SDK))"
	@mkdir -p "$(OUT)"
	$(CC) $(TEST_FLAGS) -Iextensions/shared extensions/night-shift/test.c -o "$(OUT)/test-night-shift"
	"$(OUT)/test-night-shift"
	$(CC) $(TEST_FLAGS) extensions/playback-log/test.c -o "$(OUT)/test-playback-log"
	"$(OUT)/test-playback-log"

PYTHON ?= python3
TEST_FLAGS := -std=c11 -I"$(abspath $(SDK))" -fsanitize=address,undefined \
              -fno-sanitize=alignment -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
