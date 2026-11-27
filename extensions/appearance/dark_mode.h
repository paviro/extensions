/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef DARK_MODE_H
#define DARK_MODE_H

/* Dark Mode: Auto from sunset to sunrise by the schedule, On or Off. The
 * look on the screen is the switch's business; this is what the setting
 * asks for. */
enum { DARK_AUTO, DARK_ON, DARK_OFF };

extern const char *const dark_mode_labels[3];
void dark_mode_set(int mode);
int dark_mode_auto(void);
/* Whether the setting asks for the dark appearance right now. */
int dark_mode_wanted(void);
const char *dark_mode_caption(void);

#endif
