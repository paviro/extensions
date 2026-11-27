# SPDX-License-Identifier: GPL-3.0-only
# The per-sample work is the dsp library's cascade, built as ARM there.
main_SOURCES := main.c design.c modes.c ../shared/bank.c
editor_SOURCES := editor.c design.c modes.c
EXTRA_CFLAGS := -I../shared -DBANK_STAGES=5
include $(SDK)/rv.mk
