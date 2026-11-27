# SPDX-License-Identifier: GPL-3.0-only
main_SOURCES := main.c curve.c design.c
# Every frame is about nine 32 by 32 to 64 bit multiplies: one smull each in
# ARM, a library call each in Thumb.
main_ARM_SOURCES := compressor.c
editor_SOURCES := editor.c labels.c curve.c
include $(SDK)/rv.mk
