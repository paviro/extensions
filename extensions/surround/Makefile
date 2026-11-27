# SPDX-License-Identifier: GPL-3.0-only
main_SOURCES := main.c design.c labels.c
# The loop multiplies 32 by 32 to 64 bits six times per frame: one ARM
# instruction each, a library call each in Thumb.
main_ARM_SOURCES := surround.c
editor_SOURCES := editor.c response.c design.c labels.c
EXTRA_CFLAGS := -I../shared
include $(SDK)/rv.mk
