/* uart.h — interrupt-driven UART for the HC-05 link */
#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(uint32_t baud);
void uart_putc(char c);
void uart_puts(const char *s);

/* Returns 1 and fills c when a byte is waiting, 0 otherwise. */
uint8_t uart_getc(char *c);

#endif
