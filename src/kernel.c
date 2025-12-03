#include <stdint.h>
#include <stddef.h>   /* NULL */
#include "uart.h"
#include "fs.h"       /* make sure src/fs.h exists and src/fs.c is added to Makefile */
#include "loader.h"   /* for loader_run_by_name() */

/* tiny strlen for bare-metal */
static int kstrlen(const char *s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

/* tiny strncmp for bare-metal (compare up to n chars) */
static int kstrncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca == '\0' && cb == '\0') return 0;
        if (ca == '\0' || cb == '\0') return (int)ca - (int)cb;
        if (ca != cb) return (int)ca - (int)cb;
    }
    return 0;
}


/* --- helpers --- */

static void uart_puts_nl(const char *s) {
    uart_puts(s);
    uart_putc('\n');
}

/* Read a line from UART into buf (NUL-terminated). */
static void read_line(char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        int c = uart_getc_blocking();
        if (c == '\r' || c == '\n') {
            uart_putc('\r');
            uart_putc('\n');
            break;
        } else if (c == 0x7f || c == '\b') { /* backspace */
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

/* simple string equals */
static int streq(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++; b++;
    }
    return *a == *b;
}

/* ---- fs_list_cb: print name and size (place before kmain so kmain can call it) ---- */
static void fs_list_cb(const char *name, uint32_t size, void *cookie) {
    (void)cookie;
    uart_puts("  ");
    uart_puts(name);
    uart_puts(" (");

    /* convert size to decimal string */
    if (size == 0) {
        uart_puts("0");
    } else {
        char rev[12];
        int ri = 0;
        unsigned int s = size;
        while (s) {
            rev[ri++] = '0' + (s % 10);
            s /= 10;
        }
        char tmp[12];
        int ti = 0;
        for (int j = ri - 1; j >= 0; j--) tmp[ti++] = rev[j];
        tmp[ti] = '\0';
        uart_puts(tmp);
    }

    uart_puts(" bytes)\n");
}

/* ---- main kernel entry ---- */
void kmain(void) {
    uart_puts("Simple RISC-V OS booted!\n");
    uart_puts("Type 'help' for commands.\n\n");

    /* init filesystem and create a sample file */
    fs_init();
    const char *msg = "Hello from the simple FS!\n";
    fs_create("hello.txt", (const uint8_t*)msg, (uint32_t)kstrlen(msg));

    char line[128];

    while (1) {
        uart_puts("> ");
        read_line(line, sizeof(line));

        if (streq(line, "help")) {
            uart_puts("Commands:\n");
            uart_puts("  help           - show this help\n");
            uart_puts("  echo <text>    - echo text\n");
            uart_puts("  ls             - list files\n");
            uart_puts("  cat <file>     - show file contents\n");
            uart_puts("  write <f> <t>  - write text to file (overwrite/create)\n");
            uart_puts("  rm <file>      - remove file\n");
            uart_puts("  lsprogs        - lists programs\n");
            uart_puts("  run <program>  - runs a program\n");
            uart_puts("  reboot         - not implemented (message only)\n");
        }
        else if (kstrncmp(line, "echo", 4) == 0) {
            const char *p = line + 4;
            if (*p == ' ') p++;
            uart_puts(p);
            uart_puts("\n");
        }
        else if (streq(line, "ls")) {
            uart_puts("Files:\n");
            fs_list(fs_list_cb, NULL);
        }
        else if (kstrncmp(line, "cat ", 4) == 0) {
            const char *name = line + 4;
            if (*name) {
                uint8_t buf[FS_FILE_MAX];
                int r = fs_read(name, buf, sizeof(buf));
                if (r < 0) {
                    uart_puts("File not found\n");
                } else {
                    for (int i = 0; i < r; i++) uart_putc((char)buf[i]);
                    /* ensure newline after file output */
                    uart_putc('\n');
                }
            } else {
                uart_puts("Usage: cat <filename>\n");
            }
        }
        else if (kstrncmp(line, "write ", 6) == 0) {
            /* write <name> <text> */
            char *p = line + 6;
            if (*p == '\0') {
                uart_puts("Usage: write <filename> <text>\n");
            } else {
                /* find first space separating name and text */
                char *space = p;
                while (*space && *space != ' ') space++;
                if (*space == '\0') {
                    uart_puts("Usage: write <filename> <text>\n");
                } else {
                    *space = '\0';
                    const char *name = p;
                    const char *text = space + 1;
                    fs_write(name, (const uint8_t*)text, (uint32_t)kstrlen(text));
                    uart_puts("OK\n");
                }
            }
        }
        else if (kstrncmp(line, "rm ", 3) == 0) {
            const char *name = line + 3;
            if (*name) {
                int r = fs_remove(name);
                if (r == 0) uart_puts("Removed\n");
                else uart_puts("File not found\n");
            } else {
                uart_puts("Usage: rm <filename>\n");
            }
        }
        else if (streq(line, "reboot")) {
            uart_puts("Pretending to reboot... (not implemented)\n");
        }
        else if (line[0] == '\0') {
            /* ignore empty input */
        }else if (streq(line, "lsprogs")) {
            loader_list_programs();
        } else if (line[0] == 'r' && line[1] == 'u' && line[2] == 'n' && line[3] == ' ') {

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

            if (ni == 0) {
                uart_puts("Program name empty\n");
            } else {
                int r = loader_run_by_name(namebuf);
                if (r < 0) {
                    uart_puts("Program not found: ");
                    uart_puts(namebuf);
                    uart_puts("\n");
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