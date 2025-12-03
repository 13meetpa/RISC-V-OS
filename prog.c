#include <stddef.h>
#include <stdint.h>
#include <prog.h>
#include "user_hello_bin.c" /* or better: include the generated header that declares the array */

extern const unsigned char user_hello_bin[];
extern const unsigned int user_hello_bin_len;

prog_t prog_table[] = {
    { "hello", (const uint8_t*)user_hello_bin, (size_t)user_hello_bin_len, (const void*)0x80200000 },
};

const size_t prog_table_count = 1;
