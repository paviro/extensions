# SPDX-License-Identifier: GPL-3.0-only
SDK ?= ../osos-lab/rvx-sdk
OUT ?= build
extensions := balance crossfeed dsp fast-seek long-audio-fix night-shift playback-log sleep-timer

.PHONY: all $(extensions)
all: $(extensions)

$(extensions):
	$(MAKE) -C extensions/$@ all SDK="$(abspath $(SDK))" OUT="$(abspath $(OUT))/.work/$@"
	@mkdir -p "$(OUT)/Extensions"
	cp "$(OUT)/.work/$@/$@.rvx" "$(OUT)/Extensions/$@.rvx"

.PHONY: check
check:
	$(PYTHON) extensions/balance/tests/run.py "$(abspath $(SDK))"
	$(PYTHON) extensions/crossfeed/tests/run.py "$(abspath $(SDK))"
	$(PYTHON) extensions/dsp/tests/run.py "$(abspath $(SDK))"
	@mkdir -p "$(OUT)"
	$(CC) $(TEST_FLAGS) extensions/night-shift/test.c -o "$(OUT)/test-night-shift"
	"$(OUT)/test-night-shift"
	$(CC) $(TEST_FLAGS) extensions/playback-log/test.c -o "$(OUT)/test-playback-log"
	"$(OUT)/test-playback-log"

PYTHON ?= python3
TEST_FLAGS := -std=c11 -I"$(abspath $(SDK))" -fsanitize=address,undefined \
              -fno-sanitize=alignment -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
