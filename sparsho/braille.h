/* braille.h — character to braille cell encoding (Grade-1 English) */
#ifndef BRAILLE_H
#define BRAILLE_H

#include <stdint.h>

/* A cell is a 6-bit mask. Bit n is set when dot (n+1) is raised:
 *
 *      dot1  dot4        bit0  bit3
 *      dot2  dot5   =>   bit1  bit4
 *      dot3  dot6        bit2  bit5
 *
 * The left column is bits 0..2, the right column is bits 3..5. Each column
 * therefore holds a value 0..7 — which is exactly the drum position that
 * column's motor must rotate to. See braille_columns().
 */
#define BRAILLE_NUMBER_SIGN 0x3Cu /* dots 3,4,5,6 — precedes a digit run */

/* Returns the cell mask for c, or 0 for an unsupported character.
 * Letters are folded to lowercase. Digits return the mask for their
 * matching letter (1..9,0 -> a..i,j); the caller emits the number sign. */
uint8_t braille_char(char c);

/* Splits a mask into its two drum positions, each 0..7. */
static inline uint8_t braille_left(uint8_t mask)  { return mask & 0x07u; }
static inline uint8_t braille_right(uint8_t mask) { return (mask >> 3) & 0x07u; }

/* Encodes one word into out[], inserting a number sign before a digit run.
 * Returns the number of cells written, never more than max_cells. */
uint8_t braille_word(const char *word, uint8_t len, uint8_t *out, uint8_t max_cells);

#endif
