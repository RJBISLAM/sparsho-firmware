#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#define RX_SIZE 64 /* must be a power of two */

static volatile char    rx_buf[RX_SIZE];
static volatile uint8_t rx_head, rx_tail;

void uart_init(uint32_t baud)
{
    uint16_t ubrr = (uint16_t)((F_CPU / (16UL * baud)) - 1UL);

    UBRRH = (uint8_t)(ubrr >> 8);
    UBRRL = (uint8_t)ubrr;

    /* On the ATmega32 UCSRC shares its address with UBRRH; URSEL must be
     * set in the same write or the value lands in UBRRH instead. */
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0); /* 8 data, no parity, 1 stop */
    UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);

    rx_head = rx_tail = 0;
}

ISR(USART_RXC_vect)
{
    uint8_t next = (uint8_t)((rx_head + 1u) & (RX_SIZE - 1u));
    char c = UDR;
    if (next != rx_tail) { /* a full buffer drops the byte rather than wrapping */
        rx_buf[rx_head] = c;
        rx_head = next;
    }
}

uint8_t uart_getc(char *c)
{
    if (rx_head == rx_tail) return 0;
    *c = rx_buf[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1u) & (RX_SIZE - 1u));
    return 1;
}

void uart_putc(char c)
{
    while (!(UCSRA & (1 << UDRE))) { }
    UDR = c;
}

void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}
