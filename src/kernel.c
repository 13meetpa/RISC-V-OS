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
/* ---- Shell cwd helper ---- */
static char cwd[128] = "/"; /* default root */

/* join path: if 'path' starts with '/', return it as absolute; otherwise produce joined path into outbuf */
static void join_path(const char *path, char *outbuf, int outlen) {
    if (!path || path[0] == '\0') {
        /* copy cwd */
        int i = 0;
        while (cwd[i] && i + 1 < outlen) { outbuf[i] = cwd[i]; i++; }
        outbuf[i] = '\0';
        return;
    }
    if (path[0] == '/') {
        /* absolute */
        int i = 0;
        while (path[i] && i + 1 < outlen) { outbuf[i] = path[i]; i++; }
        outbuf[i] = '\0';
        return;
    }
    /* relative: if cwd is "/" then result is "/name" else "cwd/name" */
    int ci = 0, oi = 0;
    if (cwd[0] == '/' && cwd[1] == '\0') {
        if (oi + 1 < outlen) outbuf[oi++] = '/';
    } else {
        while (cwd[ci] && oi + 1 < outlen) outbuf[oi++] = cwd[ci++];
        if (oi + 1 < outlen && outbuf[oi-1] != '/') outbuf[oi++] = '/';
    }
    int pi = 0;
    while (path[pi] && oi + 1 < outlen) outbuf[oi++] = path[pi++];
    outbuf[oi] = '\0';
}

/* cd command: change cwd if directory exists */
static int shell_cd(const char *arg) {
    char full[128];
    join_path(arg, full, sizeof(full));
    if (full[0] == '\0') return -1;
    if (full[0] != '/') {
        /* ensure leading slash */
        char tmp[128];
        tmp[0] = '/';
        int i = 1; int j = 0;
        while (full[j] && i + 1 < (int)sizeof(tmp)) tmp[i++] = full[j++];
        tmp[i] = '\0';
        for (int k = 0; k <= i; k++) full[k] = tmp[k];
    }
    int idx = fs_resolve(full);
    if (idx < 0) return -1;
    /* must be dir */
    extern fs_entry_t fs_entries[]; /* we can't reference fs_entries directly; we can rely on fs_list_dir to check */
    /* safer: test by listing */
    if (fs_list_dir(full, (fs_list_cb_t)0, NULL) != 0) return -1;
    /* copy full into cwd */
    int ci = 0;
    while (full[ci] && ci + 1 < (int)sizeof(cwd)) { cwd[ci] = full[ci]; ci++; }
    cwd[ci] = '\0';
    return 0;
}

/* pwd */
static void shell_pwd(void) {
    uart_puts(cwd);
    uart_puts("\n");
}

/* callback for listing that prints directories with a slash */
static void fs_list_cb_shell(const char *name, uint32_t size, uint8_t is_dir, void *cookie) {
    (void)cookie;
    uart_puts("  ");
    uart_puts(name);
    if (is_dir) uart_puts("/");
    uart_puts("\n");
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
    
    /* Initialize filesystem (fs_init is provided by src/fs.c) */
    fs_init();

    /* create a sample file at root */
    const char *msg = "Hello from the simple FS!\n";
    fs_write_path("/hello.txt", (const uint8_t*)msg, (uint32_t)kstrlen(msg));

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

        else if (kstrncmp(line, "ls", 2) == 0 && (line[2] == '\0' || line[2] == ' ')) {
            const char *arg = line + 2;
            if (*arg == ' ') arg++;
            char pathbuf[128];
            if (*arg == '\0') {
                join_path("", pathbuf, sizeof(pathbuf)); /* cwd */
            } else {
                join_path(arg, pathbuf, sizeof(pathbuf));
            }
            const char *listpath = pathbuf;
            if (pathbuf[0] == '\0') listpath = "/";  /* safe fallback */
            if (fs_list_dir(listpath, fs_list_cb_shell, NULL) != 0) {
                uart_puts("Not a directory or not found\n");
            }
        }

        else if (kstrncmp(line, "cat ", 4) == 0) {
            const char *name = line + 4;
            if (*name) {
                char pathbuf[128];
                join_path(name, pathbuf, sizeof(pathbuf));

                uint8_t buf[FS_FILE_MAX];
                int r = fs_read_path(pathbuf, buf, sizeof(buf));
                if (r < 0) {
                    uart_puts("File not found or is a directory\n");
                } else {
                    for (int i = 0; i < r; i++) uart_putc((char)buf[i]);
                    uart_putc('\n');
                }
            } else {
                uart_puts("Usage: cat <path>\n");
            }
        }

        /* write <path> <text> */
        else if (kstrncmp(line, "write ", 6) == 0) {
            char *p = line + 6;
            if (*p == '\0') { uart_puts("Usage: write <path> <text>\n"); }
            else {
                char *space = p;
                while (*space && *space != ' ') space++;
                if (*space == '\0') { uart_puts("Usage: write <path> <text>\n"); }
                else {
                    *space = '\0';
                    const char *name = p;
                    const char *text = space + 1;
                    char pathbuf[128];
                    join_path(name, pathbuf, sizeof(pathbuf));
                    if (fs_write_path(pathbuf, (const uint8_t*)text, (uint32_t)kstrlen(text)) == 0) uart_puts("OK\n");
                    else uart_puts("write failed\n");
                }
            }
        }

        /* rm <path> */
        else if (kstrncmp(line, "rm ", 3) == 0) {
            const char *name = line + 3;
            if (*name) {
                char pathbuf[128];
                join_path(name, pathbuf, sizeof(pathbuf));
                int r = fs_remove_path(pathbuf);
                if (r == 0) uart_puts("Removed\n");
                else uart_puts("File not found or not a file\n");
            } else {
                uart_puts("Usage: rm <path>\n");
            }
        }

        /* ls [path] */
        else if (kstrncmp(line, "ls", 2) == 0 && (line[2] == '\0' || line[2] == ' ')) {
            const char *arg = line + 2;
            if (*arg == ' ') arg++;
            char pathbuf[128];
            if (*arg == '\0') {
                /* list cwd */
                join_path("", pathbuf, sizeof(pathbuf));
            } else {
                join_path(arg, pathbuf, sizeof(pathbuf));
            }
            if (fs_list_dir(pathbuf, fs_list_cb_shell, NULL) != 0) {
                uart_puts("Not a directory or not found\n");
            }
        }

        /* mkdir <path> */
        else if (kstrncmp(line, "mkdir ", 6) == 0) {
            const char *arg = line + 6;
            if (*arg == '\0') { uart_puts("Usage: mkdir <path>\n"); }
            else {
                char pathbuf[128];
                join_path(arg, pathbuf, sizeof(pathbuf));
                if (fs_mkdir(pathbuf) == 0) uart_puts("OK\n");
                else uart_puts("mkdir failed\n");
            }
        }

        /* rmdir <path> */
        else if (kstrncmp(line, "rmdir ", 6) == 0) {
            const char *arg = line + 6;
            if (*arg == '\0') { uart_puts("Usage: rmdir <path>\n"); }
            else {
                char pathbuf[128];
                join_path(arg, pathbuf, sizeof(pathbuf));
                if (fs_rmdir(pathbuf) == 0) uart_puts("Removed\n");
                else uart_puts("rmdir failed (non-empty or not found)\n");
            }
        }

        /* cd <path> */
        else if (kstrncmp(line, "cd ", 3) == 0) {
            const char *arg = line + 3;
            if (*arg == '\0') { uart_puts("Usage: cd <path>\n"); }
            else {
                if (shell_cd(arg) == 0) uart_puts("OK\n");
                else uart_puts("cd failed\n");
            }
        }

        /* pwd */
        else if (streq(line, "pwd")) {
            shell_pwd();
        }

        /* write and cat commands: use join_path to resolve full path before calling fs_* */
        else if (kstrncmp(line, "cat ", 4) == 0) {
            const char *name = line + 4;
            if (*name) {
                char pathbuf[128];
                join_path(name, pathbuf, sizeof(pathbuf));
                uint8_t buf[FS_FILE_MAX];
                int r = fs_read_path(pathbuf, buf, sizeof(buf));
                if (r < 0) {
                    uart_puts("File not found or is a directory\n");
                } else {
                    for (int i = 0; i < r; i++) uart_putc((char)buf[i]);
                    uart_putc('\n');
                }
            } else {
                uart_puts("Usage: cat <path>\n");
            }
        }
        else if (kstrncmp(line, "write ", 6) == 0) {
            char *p = line + 6;
            if (*p == '\0') { uart_puts("Usage: write <path> <text>\n"); }
            else {
                char *space = p;
                while (*space && *space != ' ') space++;
                if (*space == '\0') { uart_puts("Usage: write <path> <text>\n"); }
                else {
                    *space = '\0';
                    const char *name = p;
                    const char *text = space + 1;
                    char pathbuf[128];
                    join_path(name, pathbuf, sizeof(pathbuf));
                    if (fs_write_path(pathbuf, (const uint8_t*)text, (uint32_t)kstrlen(text)) == 0) uart_puts("OK\n");
                    else uart_puts("write failed\n");
                }
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
                } else {
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