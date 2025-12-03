#include <stdint.h>
#include "uart.h"
#include "loader.h"




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
        } else if (streq(line, "lsprogs")) {
            loader_list_programs();
        } else if (line[0] == 'r' && line[1] == 'u' && line[2] == 'n' && line[3] == ' ') {

    uart_puts("[DEBUG] raw input after 'run ': '");
    uart_puts(line + 4);
    uart_puts("'\n");

    const char *p = line + 4;

    /* skip leading spaces */
    while (*p == ' ') p++;

    char namebuf[64];
    int ni = 0;

    /* copy characters */
    while (*p != '\0' && ni < (int)(sizeof(namebuf)-1)) {
        namebuf[ni++] = *p++;
    }

    /* trim trailing garbage */
    while (ni > 0 &&
          (namebuf[ni-1] == ' ' ||
           namebuf[ni-1] == '\r' ||
           namebuf[ni-1] == '\n' ||
           namebuf[ni-1] == '\t'))
    {
        ni--;
    }

    namebuf[ni] = '\0';

    uart_puts("[DEBUG] final parsed name: '");
    uart_puts(namebuf);
    uart_puts("'\n");

    if (ni == 0) {
        uart_puts("[DEBUG] name empty!\n");
    } else {
        int r = loader_run_by_name(namebuf);
        if (r < 0) {
            uart_puts("[DEBUG] loader_run_by_name() returned -1\n");
            uart_puts("Program not found: ");
            uart_puts(namebuf);
            uart_puts("\n");
        } else {
            uart_puts("[DEBUG] loader_run_by_name() returned success\n");
        }
    }
}

        else {
            uart_puts("Unknown command: ");
            uart_puts(line);
            uart_puts("\n");
        }
    }
}