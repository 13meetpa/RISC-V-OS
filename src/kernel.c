#include <stdint.h>
#include "uart.h"

static void read_line(char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        int c = uart_getc_blocking();
        if (c == '\r' || c == '\n') {
            uart_putc('\r');
            uart_putc('\n');
            break;
        } else if (c == 0x7f || c == '\b') { // backspace
            if (i > 0) {
                i--;
                uart_puts("\b \b");
            }
        } else {
            buf[i++] = (char)c;
            uart_putc((char)c);
        }
    }
    buf[i] = '\0';
}

static int streq(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++; b++;
    }
    return *a == *b;
}

void kmain(void) {
    uart_puts("Simple RISC-V OS booted!\n");
    uart_puts("Type 'help' for commands.\n\n");

    char line[128];

    while (1) {
        uart_puts("> ");
        read_line(line, sizeof(line));

        if (streq(line, "help")) {
            uart_puts("Commands:\n");
            uart_puts("  help     - show this help\n");
            uart_puts("  echo ... - echo text\n");
            uart_puts("  reboot   - (not implemented, just message)\n");
        } else if (line[0] == 'e' && line[1] == 'c' && line[2] == 'h' && line[3] == 'o' && (line[4] == ' ' || line[4] == '\0')) {
            uart_puts(line + 5);
            uart_puts("\n");
        } else if (streq(line, "reboot")) {
            uart_puts("Pretending to reboot... (not implemented)\n");
        } else if (line[0] == '\0') {
            // empty line: ignore
        } else {
            uart_puts("Unknown command: ");
            uart_puts(line);
            uart_puts("\n");
        }
    }
}
