#ifndef SIMPLE_FS_H
#define SIMPLE_FS_H

#include <stdint.h>

#define FS_MAX_FILES    32
#define FS_NAME_MAX     16
#define FS_FILE_MAX     (4*1024)  /* 4 KiB per file */

typedef struct {
    char name[FS_NAME_MAX];
    uint32_t size;
    uint8_t data[FS_FILE_MAX];
    uint8_t used; /* 0 = free, 1 = used */
} fs_entry_t;

/* Initialize filesystem (clears entries). */
void fs_init(void);

/* Create a new file with initial data (len may be 0). Returns 0 on success, -1 on failure. */
int fs_create(const char *name, const uint8_t *data, uint32_t len);

/* Overwrite (or create) file contents. Returns 0 success, -1 fail. */
int fs_write(const char *name, const uint8_t *data, uint32_t len);

/* Read file into buffer. Returns bytes read, or -1 if not found. */
int fs_read(const char *name, uint8_t *buf, uint32_t bufsize);

/* Remove a file. Returns 0 success, -1 if not found. */
int fs_remove(const char *name);

/* Produce a listing into the provided callback: callback(name, size) */
typedef void (*fs_list_cb_t)(const char *name, uint32_t size, void *cookie);
void fs_list(fs_list_cb_t cb, void *cookie);

#endif
