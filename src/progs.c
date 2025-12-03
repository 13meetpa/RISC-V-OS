/* src/progs.c - program table using const length from xxd-generated array */
#include <stddef.h>
#include <stdint.h>
#include "loader.h"

/* xxd generates:
   unsigned char user_hello_bin[] = { ... };
   const unsigned int user_hello_bin_len = N;
*/
extern const unsigned char user_hello_bin[];
extern const unsigned int user_hello_bin_len;

prog_t prog_table[] = {
    { "hello", (const uint8_t*)user_hello_bin, (size_t)user_hello_bin_len, (const void*)0x80200000 },
};

size_t prog_table_count = sizeof(prog_table) / sizeof(prog_table[0]);
