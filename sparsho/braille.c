#include "braille.h"
#include <avr/pgmspace.h>

/* a..z, standard Grade-1 English braille */
static const uint8_t letter_mask[26] PROGMEM = {
    0x01, 0x03, 0x09, 0x19, 0x11, 0x0B, 0x1B, 0x13, 0x0A, /* a i */
    0x1A, 0x05, 0x07, 0x0D, 0x1D, 0x15, 0x0F, 0x1F, 0x17, /* j r */
    0x0E, 0x1E, 0x25, 0x27, 0x3A, 0x2D, 0x3D, 0x35        /* s z */
};

/* digits reuse the letters a..j: 1->a, 2->b, ... 9->i, 0->j */
static const char digit_letter[10] = { 'j', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i' };

uint8_t braille_char(char c)
{
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    if (c >= 'a' && c <= 'z') return pgm_read_byte(&letter_mask[c - 'a']);
    if (c >= '0' && c <= '9') {
        char l = digit_letter[c - '0'];
        return pgm_read_byte(&letter_mask[l - 'a']);
    }
    return 0;
}

uint8_t braille_word(const char *word, uint8_t len, uint8_t *out, uint8_t max_cells)
{
    uint8_t n = 0;
    uint8_t in_number = 0;

    for (uint8_t i = 0; i < len && n < max_cells; i++) {
        char c = word[i];

        if (c >= '0' && c <= '9') {
            if (!in_number) {
                out[n++] = BRAILLE_NUMBER_SIGN;
                in_number = 1;
                if (n >= max_cells) break;
            }
        } else {
            in_number = 0;
        }

        uint8_t m = braille_char(c);
        if (m) out[n++] = m;
    }
    return n;
}
