/* Sparsho — refreshable braille display, ATmega32 @ 16 MHz
 *
 * The phone does the speech recognition, the AI call and the OCR, then sends
 * plain text over Bluetooth. Everything from that text to the moving dots
 * happens here.
 *
 * Protocol, 9600 8N1, lines terminated with '\n':
 *   in    "hello world\n"   text to display
 *   out   "R\n"             booted, drums homed, ready
 *   out   "W:2/5\n"         now showing chunk 2 of 5
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

#include "uart.h"
#include "braille.h"
#include "display.h"

#define MAX_TEXT    200
#define MAX_MASKS   240
#define MAX_CHUNKS   64

/* Buttons are wired to ground and use the internal pull-ups, so a pressed
 * button reads low. */
#define BTN_PORT   PORTD
#define BTN_PIN    PIND
#define BTN_DDR    DDRD
#define BTN_NEXT   PD2
#define BTN_PREV   PD3
#define BTN_REPEAT PD4
#define BTN_MODE   PD5
#define BUZZER     PD6
#define BTN_MASK   ((1 << BTN_NEXT) | (1 << BTN_PREV) | (1 << BTN_REPEAT) | (1 << BTN_MODE))

static char    line[MAX_TEXT];
static uint8_t line_len;

static uint8_t masks[MAX_MASKS];
static uint8_t chunk_start[MAX_CHUNKS];
static uint8_t chunk_len[MAX_CHUNKS];
static uint8_t chunk_count;
static uint8_t chunk_index;

static void beep(uint16_t ms)
{
    BTN_PORT |= (1 << BUZZER);
    while (ms--) _delay_ms(1);
    BTN_PORT &= (uint8_t)~(1 << BUZZER);
}

static void report_position(void)
{
    char msg[16];
    uint8_t n = 0;
    msg[n++] = 'W'; msg[n++] = ':';
    msg[n++] = (char)('0' + ((chunk_index + 1) / 10));
    msg[n++] = (char)('0' + ((chunk_index + 1) % 10));
    msg[n++] = '/';
    msg[n++] = (char)('0' + (chunk_count / 10));
    msg[n++] = (char)('0' + (chunk_count % 10));
    msg[n++] = '\n';
    msg[n]   = '\0';
    uart_puts(msg);
}

static void show_chunk(void)
{
    if (!chunk_count) return;
    display_set(&masks[chunk_start[chunk_index]], chunk_len[chunk_index]);
    report_position();
}

/* Turns a line of text into cells, then cuts those cells into chunks that
 * each fit the display. A word shorter than the display becomes one chunk,
 * so a whole word is felt at once; a longer word is split across several. */
static void build_chunks(void)
{
    uint8_t total = 0;
    chunk_count = 0;

    uint8_t i = 0;
    while (i < line_len && chunk_count < MAX_CHUNKS) {
        while (i < line_len && line[i] == ' ') i++;      /* skip separators */
        uint8_t start = i;
        while (i < line_len && line[i] != ' ') i++;      /* one word */
        uint8_t wlen = (uint8_t)(i - start);
        if (!wlen) break;

        uint8_t space = (uint8_t)(MAX_MASKS - total);
        uint8_t cells = braille_word(&line[start], wlen, &masks[total], space);

        uint8_t done = 0;
        while (done < cells && chunk_count < MAX_CHUNKS) {
            uint8_t take = (uint8_t)(cells - done);
            if (take > NUM_CELLS) take = NUM_CELLS;
            chunk_start[chunk_count] = (uint8_t)(total + done);
            chunk_len[chunk_count]   = take;
            chunk_count++;
            done = (uint8_t)(done + take);
        }
        total = (uint8_t)(total + cells);
    }

    chunk_index = 0;
}

/* One debounced falling edge per press. */
static uint8_t button_pressed(void)
{
    static uint8_t stable = BTN_MASK;
    uint8_t now = (uint8_t)(BTN_PIN & BTN_MASK);

    if (now == stable) return 0;
    _delay_ms(25);
    if ((BTN_PIN & BTN_MASK) != now) return 0;

    uint8_t went_low = (uint8_t)(stable & ~now);
    stable = now;
    return went_low;
}

void sparsho_run(void)
{
    BTN_DDR  &= (uint8_t)~BTN_MASK;   /* buttons are inputs */
    BTN_PORT |= BTN_MASK;             /* with pull-ups */
    BTN_DDR  |= (1 << BUZZER);

    uart_init(9600);
    display_init();
    sei();

    display_home();
    beep(120);
    uart_puts("R\n");

    for (;;) {
        char c;
        while (uart_getc(&c)) {
            if (c == '\r') continue;
            if (c == '\n') {
                line[line_len] = '\0';
                build_chunks();
                line_len = 0;
                if (chunk_count) { beep(60); show_chunk(); }
            } else if (line_len < MAX_TEXT - 1) {
                line[line_len++] = c;
            }
            /* A line longer than the buffer keeps its first MAX_TEXT-1
             * characters; the rest is dropped rather than wrapping. */
        }

        uint8_t press = button_pressed();
        if (press && chunk_count) {
            if ((press & (1 << BTN_NEXT)) && chunk_index + 1 < chunk_count) {
                chunk_index++; show_chunk(); beep(30);
            } else if ((press & (1 << BTN_PREV)) && chunk_index > 0) {
                chunk_index--; show_chunk(); beep(30);
            } else if (press & (1 << BTN_REPEAT)) {
                show_chunk(); beep(30);
            } else if (press & (1 << BTN_MODE)) {
                uart_puts("M\n");   /* the phone app decides what a mode change means */
                beep(30); beep(30);
            }
        }
    }
}
