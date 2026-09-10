/* display.h — the braille surface, independent of how the dots are moved.
 *
 * main.c only ever calls these four functions. Swapping the mechanism
 * (cam drums vs a travelling carriage) means replacing display_*.c and
 * nothing else.
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#ifndef NUM_CELLS
#define NUM_CELLS 2 /* matches the 2-cell / 4-motor hardware currently wired */
#endif

void display_init(void);

/* Drives every drum to its home position. Blocks until finished.
 * Must run once at power-up: the firmware cannot know where the drums
 * were left when power was removed. */
void display_home(void);

/* Requests a new pattern. Returns immediately; the motors are stepped in
 * the background by the timer interrupt. */
void display_set(const uint8_t *masks, uint8_t count);

/* 1 once every motor has reached its target. */
uint8_t display_ready(void);

#endif
