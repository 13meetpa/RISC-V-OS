/* src/loader.h */
#ifndef LOADER_H
#define LOADER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    const uint8_t *data;
    size_t size;
    const void *entry;
} prog_t;

/* Provided by progs.c */
extern prog_t prog_table[];
extern size_t prog_table_count;

void loader_list_programs(void);
int loader_run_by_name(const char *name);

#endif /* LOADER_H */

