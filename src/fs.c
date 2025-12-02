/* src/fs.c - simple in-memory filesystem (bare-metal safe) */
#include "fs.h"
#include <stdint.h>

/* ---- small helpers because no libc is available ---- */

/* tiny memcpy implementation */
static void *kmemcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

/* tiny strlen */
static uint32_t kstrlen(const char *s) {
    uint32_t n = 0;
    while (s && s[n]) n++;
    return n;
}

/* tiny strncmp: compare up to n chars; return 0 if equal for up to n */
static int kstrncmp(const char *a, const char *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca == '\0' && cb == '\0') return 0;
        if (ca == '\0' || cb == '\0') return (int)ca - (int)cb;
        if (ca != cb) return (int)ca - (int)cb;
    }
    return 0;
}

/* tiny strncpy: copies up to n-1 and NUL-terminates (like strncpy but safer) */
static void kstrncpy(char *dst, const char *src, uint32_t n) {
    if (n == 0) return;
    uint32_t i = 0;
    for (; i + 1 < n && src[i]; i++) dst[i] = src[i];
    /* NUL-terminate */
    dst[i] = '\0';
}

/* ---- filesystem implementation ---- */

static fs_entry_t fs_entries[FS_MAX_FILES];

void fs_init(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_entries[i].used = 0;
        fs_entries[i].size = 0;
        fs_entries[i].name[0] = '\0';
    }
}

static int find_entry(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_entries[i].used && kstrncmp(fs_entries[i].name, name, FS_NAME_MAX) == 0)
            return i;
    }
    return -1;
}

static int find_free(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!fs_entries[i].used) return i;
    }
    return -1;
}

int fs_create(const char *name, const uint8_t *data, uint32_t len) {
    if (!name) return -1;
    uint32_t namelen = kstrlen(name);
    if (namelen == 0 || namelen >= FS_NAME_MAX) return -1;
    if (len > FS_FILE_MAX) return -1;
    if (find_entry(name) >= 0) return -1; /* already exists */

    int idx = find_free();
    if (idx < 0) return -1;

    kstrncpy(fs_entries[idx].name, name, FS_NAME_MAX);
    fs_entries[idx].size = len;
    if (len && data) kmemcpy(fs_entries[idx].data, data, len);
    fs_entries[idx].used = 1;
    return 0;
}

int fs_write(const char *name, const uint8_t *data, uint32_t len) {
    if (!name) return -1;
    if (len > FS_FILE_MAX) return -1;
    int idx = find_entry(name);
    if (idx >= 0) {
        /* overwrite */
        fs_entries[idx].size = len;
        if (len && data) kmemcpy(fs_entries[idx].data, data, len);
        return 0;
    } else {
        /* create */
        return fs_create(name, data, len);
    }
}

int fs_read(const char *name, uint8_t *buf, uint32_t bufsize) {
    int idx = find_entry(name);
    if (idx < 0) return -1;
    uint32_t tocopy = fs_entries[idx].size;
    if (tocopy > bufsize) tocopy = bufsize;
    if (tocopy && buf) kmemcpy(buf, fs_entries[idx].data, tocopy);
    return (int)tocopy;
}

int fs_remove(const char *name) {
    int idx = find_entry(name);
    if (idx < 0) return -1;
    fs_entries[idx].used = 0;
    fs_entries[idx].name[0] = '\0';
    fs_entries[idx].size = 0;
    return 0;
}

void fs_list(fs_list_cb_t cb, void *cookie) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_entries[i].used) {
            cb(fs_entries[i].name, fs_entries[i].size, cookie);
        }
    }
}
