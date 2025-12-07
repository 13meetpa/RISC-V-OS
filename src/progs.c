/* src/progs.c */
#include "progs.h"

/* This file must contain the actual array definition:
   e.g. unsigned char user_hello_bin[] = {0x7f, 0x45, ... };
   Many tools produce a .c file that defines the array; include it here. */
#include "user_hello_bin.c"   /* <-- ensure this path is correct relative to src/ */

/* Now sizeof(user_hello_bin) is an integer constant expression. */
prog_t prog_table[] = {
    { "hello", (const uint8_t*)user_hello_bin, (size_t)sizeof(user_hello_bin), (const void*)0x80200000 },
};

const size_t prog_table_count = sizeof(prog_table) / sizeof(prog_table[0]);
