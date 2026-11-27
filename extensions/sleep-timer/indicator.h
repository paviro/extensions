/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef SLEEP_TIMER_INDICATOR_H
#define SLEEP_TIMER_INDICATOR_H
#include "rv_api.h"
#include <stdint.h>

/* The Now Playing shuffle image with the timer before it: the minutes left
 * and a moon, then `shuffle` (a stock BMap, NULL when shuffle is off) 5 px to
 * their right. A BMap in a block from `api`, NULL when the heap is
 * full; freed with indicator_free. UI task. */
const void *indicator_build(const struct rv_api *api, rv_extension_handle self,
                            uint32_t minutes, const void *shuffle);
void indicator_free(const struct rv_api *api, rv_extension_handle self);

#endif
