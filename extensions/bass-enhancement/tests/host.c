/* SPDX-License-Identifier: GPL-3.0-only */

/* The engine as the tone test drives it: the shared bank over interrupt
 * masking that does nothing, fed the set layout the test builds. */

#include "bass.h"
#include "bank.h"
#include "rv_libc.h"

static uint32_t irq_save(void)
{
    return 0;
}

static void irq_restore(uint32_t state)
{
    (void)state;
}

static const struct rv_api api = {
    .size = sizeof api, .irq_save = irq_save, .irq_restore = irq_restore};

static struct bank bank = {.effect.api = &api};

struct test_set {
    uint8_t strength;
    int32_t coefficients[BANKS][BASS_STAGES][5];
};

void bass_publish(const struct test_set *chosen)
{
    struct bank_set set = {.on = chosen->strength != 0};
    memcpy(set.coefficients[0], chosen->coefficients[0],
           sizeof chosen->coefficients[0]);
    memcpy(set.coefficients[1], chosen->coefficients[1],
           sizeof chosen->coefficients[1]);
    bank_publish(&bank, &set);
}

void bass_process(int16_t *samples, uint32_t frames, uint32_t rate)
{
    bank_process(&bank, samples, frames, rate);
}
