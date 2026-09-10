/* display_drum.c — cam-drum mechanism (options A and C).
 *
 * Two drums per cell, one per dot column. A column of three dots has eight
 * possible patterns, so each drum has eight angular positions — and the
 * position number IS the pattern, because the drum's three cam tracks are
 * cut so that track n has a bump wherever bit n of the position is set.
 *
 * All drums are driven through a chain of 74HC595 shift registers: three
 * MCU pins carry 4 * NUM_DRUMS coil lines. Timer1 steps a few motors at a
 * time so the supply current stays bounded.
 */
#include "display.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define NUM_DRUMS       (NUM_CELLS * 2)
#define CHAIN_BYTES     (NUM_DRUMS / 2)  /* 4 bits per drum, 8 bits per register */

#define STEPS_PER_POS   512   /* 4096 half-steps per output revolution / 8 positions */
/* Motors moving at once. A 28BYJ-48 pulls about 200 mA while stepping, so
 * four of them plus the logic stays under 1 A — comfortable on a 5 V 2 A
 * supply. Raise this only with a bigger supply. */
#define MAX_CONCURRENT  4

/* 74HC595 chain on PORTB */
#define SR_PORT   PORTB
#define SR_DDR    DDRB
#define SR_DATA   PB0
#define SR_CLOCK  PB1
#define SR_LATCH  PB2

/* Half-step sequence for the ULN2003's four outputs. */
static const uint8_t half_step[8] = {
    0x08, 0x0C, 0x04, 0x06, 0x02, 0x03, 0x01, 0x09
};

static volatile uint8_t  phase[NUM_DRUMS];    /* 0..7, where in the coil sequence */
static volatile uint8_t  position[NUM_DRUMS]; /* 0..7, which pattern is showing */
static volatile int16_t  remaining[NUM_DRUMS];/* half-steps still owed, signed */
static volatile uint8_t  moving;

static void shift_out(void)
{
    uint8_t frame[CHAIN_BYTES];

    for (uint8_t i = 0; i < CHAIN_BYTES; i++) {
        uint8_t lo = half_step[phase[i * 2]     & 7u];
        uint8_t hi = half_step[phase[i * 2 + 1] & 7u];
        frame[i] = (uint8_t)(lo | (hi << 4));
    }

    /* The last register in the chain has to be clocked in first. */
    for (uint8_t i = CHAIN_BYTES; i > 0; i--) {
        uint8_t b = frame[i - 1];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (b & 0x80u) SR_PORT |= (1 << SR_DATA);
            else           SR_PORT &= (uint8_t)~(1 << SR_DATA);
            SR_PORT |= (1 << SR_CLOCK);
            SR_PORT &= (uint8_t)~(1 << SR_CLOCK);
            b = (uint8_t)(b << 1);
        }
    }
    SR_PORT |= (1 << SR_LATCH);
    SR_PORT &= (uint8_t)~(1 << SR_LATCH);
}

/* Cuts every coil so nothing sits energised between updates. The 64:1
 * gearbox holds the drums mechanically, so the dots do not sag. */
static void coils_off(void)
{
    /* phase[] is left alone so the next move resumes from the right point
     * in the coil sequence; only the outputs are cleared. */
    SR_PORT &= (uint8_t)~(1 << SR_DATA);
    for (uint8_t i = 0; i < CHAIN_BYTES * 8; i++) {
        SR_PORT |= (1 << SR_CLOCK);
        SR_PORT &= (uint8_t)~(1 << SR_CLOCK);
    }
    SR_PORT |= (1 << SR_LATCH);
    SR_PORT &= (uint8_t)~(1 << SR_LATCH);
}

void display_init(void)
{
    SR_DDR |= (1 << SR_DATA) | (1 << SR_CLOCK) | (1 << SR_LATCH);

    for (uint8_t i = 0; i < NUM_DRUMS; i++) {
        phase[i] = 0;
        position[i] = 0;
        remaining[i] = 0;
    }
    moving = 0;

    /* Timer1, CTC, 1 kHz tick: 16 MHz / 64 / 250 */
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
    OCR1A  = 249;
    TIMSK |= (1 << OCIE1A);
}

void display_home(void)
{
    /* There is no home sensor. Every drum is driven backwards past a full
     * revolution so it runs into its hard stop and stalls there; a geared
     * stepper simply skips steps against a stop without harm. Wherever the
     * drums started, they all finish at the stop, which is position 0. */
    cli();
    for (uint8_t i = 0; i < NUM_DRUMS; i++) remaining[i] = -(int16_t)(STEPS_PER_POS * 9);
    moving = 1;
    sei();

    while (!display_ready()) { }
    cli();
    for (uint8_t i = 0; i < NUM_DRUMS; i++) position[i] = 0;
    sei();
}

void display_set(const uint8_t *masks, uint8_t count)
{
    cli();
    for (uint8_t c = 0; c < NUM_CELLS; c++) {
        uint8_t mask = (c < count) ? masks[c] : 0u;
        uint8_t want[2] = { (uint8_t)(mask & 0x07u), (uint8_t)((mask >> 3) & 0x07u) };

        for (uint8_t half = 0; half < 2; half++) {
            uint8_t d = (uint8_t)(c * 2 + half);
            int8_t delta = (int8_t)(want[half] - position[d]);

            /* Take the short way round: never turn more than half a turn. */
            if (delta >  4) delta = (int8_t)(delta - 8);
            if (delta < -4) delta = (int8_t)(delta + 8);

            /* Any steps still owed from an interrupted move are carried
             * over, so a new word can be requested mid-motion. */
            remaining[d] = (int16_t)((int16_t)delta * STEPS_PER_POS + remaining[d]);
            position[d]  = want[half];
        }
    }
    moving = 1;
    sei();
}

uint8_t display_ready(void)
{
    return moving ? 0u : 1u;
}

ISR(TIMER1_COMPA_vect)
{
    if (!moving) return;

    uint8_t stepped = 0;
    uint8_t busy = 0;

    for (uint8_t i = 0; i < NUM_DRUMS; i++) {
        if (remaining[i] == 0) continue;
        busy = 1;
        if (stepped >= MAX_CONCURRENT) continue;

        if (remaining[i] > 0) { phase[i] = (uint8_t)((phase[i] + 1u) & 7u); remaining[i]--; }
        else                  { phase[i] = (uint8_t)((phase[i] + 7u) & 7u); remaining[i]++; }
        stepped++;
    }

    if (stepped) {
        shift_out();
    } else if (!busy) {
        moving = 0;
        coils_off();
    }
}
