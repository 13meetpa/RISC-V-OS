// uart.c
#include <stdint.h>
#include "uart.h"

#define UART0_BASE 0x10000000UL
#define UART_THR   0x00  // transmit holding
#define UART_LSR   0x05  // line status
#define UART_LSR_THRE 0x20  // transmitter holding register empty
#define UART_RBR   0x00  // receiver buffer
#define UART_LSR_DR  0x01  // data ready

static inline uint8_t mmio_read8(uintptr_t addr) {
    return *(volatile uint8_t *)addr;
}

static inline void mmio_write8(uintptr_t addr, uint8_t val) {
    *(volatile uint8_t *)addr = val;
}

void uart_putc(char c) {
    // wait until THR empty
    while ((mmio_read8(UART0_BASE + UART_LSR) & UART_LSR_THRE) == 0)
        ;
    mmio_write8(UART0_BASE + UART_THR, c);
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

int uart_getc_blocking(void) {
    // wait until data ready
    while ((mmio_read8(UART0_BASE + UART_LSR) & UART_LSR_DR) == 0)
        ;
    return mmio_read8(UART0_BASE + UART_RBR);
}
