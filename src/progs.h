/* src/progs.h */
#ifndef PROGS_H
#define PROGS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    const uint8_t *data;
    size_t size;
    const void *load_addr;
} prog_t;

extern prog_t prog_table[];
extern const size_t prog_table_count;

#endif /* PROGS_H */
