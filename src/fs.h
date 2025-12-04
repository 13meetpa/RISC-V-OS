#ifndef SIMPLE_FS_H
#define SIMPLE_FS_H

#include <stdint.h>

#define FS_MAX_FILES    64
#define FS_NAME_MAX     16
#define FS_FILE_MAX     (4*1024)  /* 4 KiB per file */

/* file / directory entry */
typedef struct {
    char name[FS_NAME_MAX];   /* single component name, not full path */
    uint32_t size;            /* for files only */
    uint8_t data[FS_FILE_MAX];
    uint8_t used;             /* 0 = free, 1 = used */
    uint8_t is_dir;           /* 1 = directory, 0 = file */
    int16_t parent;           /* index of parent entry (root = 0) */
} fs_entry_t;

/* Initialize filesystem (clears entries and creates root dir). */
void fs_init(void);

/* Create or overwrite a file by full path ("/a/b.txt" or "relative.txt"). */
int fs_write_path(const char *path, const uint8_t *data, uint32_t len);

/* Read file by path into buffer. Returns bytes read, or -1 if not found. */
int fs_read_path(const char *path, uint8_t *buf, uint32_t bufsize);

/* Make directory by path (creates single directory, creating parent must exist). */
int fs_mkdir(const char *path);

/* Remove file by path (use fs_rmdir for directories). Returns 0 success, -1 fail. */
int fs_remove_path(const char *path);

/* Remove directory if empty */
int fs_rmdir(const char *path);

/* Make a listing of a directory (path -> callback with name & size & is_dir) */
typedef void (*fs_list_cb_t)(const char *name, uint32_t size, uint8_t is_dir, void *cookie);
int fs_list_dir(const char *path, fs_list_cb_t cb, void *cookie);

/* Utility: resolve path to entry index; returns -1 if not found */
int fs_resolve(const char *path);

/* Utility: create file by name in parent index (for internal use) */
int fs_create_in_parent(int parent_idx, const char *name, const uint8_t *data, uint32_t len, uint8_t is_dir);

#endif