/* SPDX-License-Identifier: GPL-3.0-only */

#include "dark_mode.h"
#include "look.h"
#include "schedule.h"
#include "style.h"

const char *const dark_mode_labels[3] = {"Auto", "On", "Off"};
static int choice = DARK_OFF;

void dark_mode_set(int value)
{
    choice = value;
}

int dark_mode_auto(void)
{
    return choice == DARK_AUTO;
}

int dark_mode_wanted(void)
{
    struct schedule s;
    if (choice != DARK_AUTO)
        return choice == DARK_ON;
    schedule_read(&s);
    return schedule_night(&s);
}

const char *dark_mode_caption(void)
{
    const char *problem = look_problem(PANE_DARK), *note = look_appearance_note();
    if (problem)
        return problem;
    if (note)
        return note;
    if (choice == DARK_AUTO)
        return schedule_caption();
    return style_dark() ? "On" : "Off";
}
