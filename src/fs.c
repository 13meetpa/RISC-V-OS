/* src/fs.c - simple in-memory hierarchical filesystem */
#include "fs.h"
#include <stddef.h>   /* for NULL */
#include <stdint.h>
/* tiny helpers to print numbers via UART - these use uart_puts/uart_putc */
#include "uart.h"   /* make sure uart.h is available */

/* ---- small helpers to avoid libc ---- */
static void *kmemcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

static uint32_t kstrlen(const char *s) {
    uint32_t n = 0;
    while (s && s[n]) n++;
    return n;
}

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

static void kstrncpy(char *dst, const char *src, uint32_t n) {
    if (n == 0) return;
    uint32_t i = 0;
    for (; i + 1 < n && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* ---- FS storage ---- */
static fs_entry_t fs_entries[FS_MAX_FILES];

/* print unsigned int as decimal */
static void print_u32(uint32_t v) {
    if (v == 0) { uart_puts("0"); return; }
    char rev[12]; int ri = 0;
    while (v) { rev[ri++] = '0' + (v % 10); v /= 10; }
    char tmp[12]; int ti = 0;
    for (int i = ri - 1; i >= 0; i--) tmp[ti++] = rev[i];
    tmp[ti] = '\0';
    uart_puts(tmp);
}

/* helper: find free slot */
static int find_free(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!fs_entries[i].used) return i;
    }
    return -1;
}

/* helper: find child by name under parent index */
static int find_child(int parent_idx, const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_entries[i].used && fs_entries[i].parent == parent_idx) {
            if (kstrncmp(fs_entries[i].name, name, FS_NAME_MAX) == 0) return i;
        }
    }
    return -1;
}

/* helper: split next component from path.
   in: p points to component (no leading '/'), out: copy comp into compbuf (size FS_NAME_MAX),
   returns pointer after component (skip any '/') or NULL if component invalid.
*/
static const char *get_next_component(const char *p, char *compbuf) {
    if (!p || *p == '\0') return NULL;
    uint32_t i = 0;
    while (*p != '\0' && *p != '/') {
        if (i + 1 < FS_NAME_MAX) compbuf[i++] = *p;
        else return NULL; /* component too long */
        p++;
    }
    compbuf[i] = '\0';
    if (*p == '/') return p + 1;
    return p;
}

/* Initialize: clear and create root (index 0) */
void fs_init(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        fs_entries[i].used = 0;
        fs_entries[i].size = 0;
        fs_entries[i].name[0] = '\0';
        fs_entries[i].is_dir = 0;
        fs_entries[i].parent = -1;
    }
    /* reserve index 0 as root directory */
    fs_entries[0].used = 1;
    kstrncpy(fs_entries[0].name, "/", FS_NAME_MAX);
    fs_entries[0].is_dir = 1;
    /* make root's parent -1 so it is NOT listed as a child of itself */
    fs_entries[0].parent = -1;
    fs_entries[0].size = 0;
}

/* Resolve path: absolute (/a/b) or relative (no cwd handled here) starting at root.
   Returns entry index or -1 if not found
   Note: accepts "/" -> returns root
*/
int fs_resolve(const char *path) {
    if (!path) return -1;
    if (path[0] == '\0') return -1;
    if (path[0] == '/' && (path[1] == '\0')) return 0; /* root */

    int cur = 0; /* start at root for absolute paths or implicit root for relative */
    const char *p = path;
    if (p[0] == '/') p++; /* skip leading slash */

    char comp[FS_NAME_MAX];
    const char *next = p;
    while (next && *next) {
        next = get_next_component(next, comp);
        if (!comp[0]) return -1;
        int child = find_child(cur, comp);
        if (child < 0) return -1;
        cur = child;
        if (next && *next == '\0') break;
    }
    return cur;
}

/* Internal: create an entry in parent index (file or dir) */
int fs_create_in_parent(int parent_idx, const char *name, const uint8_t *data, uint32_t len, uint8_t is_dir) {
    if (!name || name[0] == '\0') return -1;
    if (kstrlen(name) >= FS_NAME_MAX) return -1;
    if (parent_idx < 0 || parent_idx >= FS_MAX_FILES) return -1;
    if (!fs_entries[parent_idx].used || !fs_entries[parent_idx].is_dir) return -1;

    if (find_child(parent_idx, name) >= 0) return -1; /* exists */

    int idx = find_free();
    if (idx < 0) return -1;

    kstrncpy(fs_entries[idx].name, name, FS_NAME_MAX);
    fs_entries[idx].used = 1;
    fs_entries[idx].is_dir = (is_dir ? 1 : 0);
    fs_entries[idx].parent = parent_idx;
    if (!is_dir) {
        if (len > FS_FILE_MAX) return -1;
        fs_entries[idx].size = len;
        if (len && data) kmemcpy(fs_entries[idx].data, data, len);
    } else {
        fs_entries[idx].size = 0;
    }
    return 0;
}

/* Resolve parent dir and filename for a path like "/a/b/c.txt" */
/* On success: *parent_idx set, name_out filled, return 0. On failure return -1. */
static int resolve_parent_and_name(const char *path, int *parent_idx, char *name_out) {
    if (!path || !parent_idx || !name_out) return -1;
    /* strip trailing slashes */
    uint32_t plen = kstrlen(path);
    if (plen == 0) return -1;

    /* Handle absolute path */
    const char *p = path;
    int cur = 0; /* start at root */
    if (p[0] == '/') p++; /* skip leading slash */

    char comp[FS_NAME_MAX];
    const char *next = p;
    const char *last_comp = NULL;
    int last_parent = cur;

    while (next && *next) {
        last_parent = cur;
        next = get_next_component(next, comp);
        if (!comp[0]) return -1;
        /* if there's more after this, descend; else this comp is the final name */
        if (next && *next) {
            int child = find_child(cur, comp);
            if (child < 0) return -1;
            if (!fs_entries[child].is_dir) return -1;
            cur = child;
        } else {
            /* final component */
            kstrncpy(name_out, comp, FS_NAME_MAX);
            *parent_idx = cur;
            return 0;
        }
    }
    /* If path ended without components (like "/" ) */
    return -1;
}

/* Create directory by path */
int fs_mkdir(const char *path) {
    int parent;
    char name[FS_NAME_MAX];
    if (resolve_parent_and_name(path, &parent, name) != 0) return -1;
    return fs_create_in_parent(parent, name, NULL, 0, 1);
}

/* Create or overwrite file by path */
int fs_write_path(const char *path, const uint8_t *data, uint32_t len) {
    int parent;
    char name[FS_NAME_MAX];
    if (resolve_parent_and_name(path, &parent, name) != 0) return -1;
    int existing = find_child(parent, name);
    if (existing >= 0) {
        /* if existing is dir, fail */
        if (fs_entries[existing].is_dir) return -1;
        if (len > FS_FILE_MAX) return -1;
        fs_entries[existing].size = len;
        if (len && data) kmemcpy(fs_entries[existing].data, data, len);
        return 0;
    } else {
        return fs_create_in_parent(parent, name, data, len, 0);
    }
}

/* Read file by path */
int fs_read_path(const char *path, uint8_t *buf, uint32_t bufsize) {
    int idx = fs_resolve(path);
    if (idx < 0) return -1;
    if (fs_entries[idx].is_dir) return -1;
    uint32_t tocopy = fs_entries[idx].size;
    if (tocopy > bufsize) tocopy = bufsize;
    if (tocopy && buf) kmemcpy(buf, fs_entries[idx].data, tocopy);
    return (int)tocopy;
}

/* Remove file (path) */
int fs_remove_path(const char *path) {
    int idx = fs_resolve(path);
    if (idx < 0) return -1;
    if (fs_entries[idx].is_dir) return -1;
    fs_entries[idx].used = 0;
    fs_entries[idx].name[0] = '\0';
    fs_entries[idx].size = 0;
    fs_entries[idx].parent = -1;
    return 0;
}

/* Remove directory if empty */
int fs_rmdir(const char *path) {
    int idx = fs_resolve(path);
    if (idx < 0) return -1;
    if (!fs_entries[idx].is_dir) return -1;
    /* check children */
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_entries[i].used && fs_entries[i].parent == idx) return -1; /* not empty */
    }
    if (idx == 0) return -1; /* cannot remove root */
    fs_entries[idx].used = 0;
    fs_entries[idx].name[0] = '\0';
    fs_entries[idx].parent = -1;
    return 0;
}

/* List directory contents by path */
int fs_list_dir(const char *path, fs_list_cb_t cb, void *cookie) {
    int idx;
    if (!path || path[0] == '\0') idx = 0;
    else idx = fs_resolve(path);
    if (idx < 0) return -1;
    if (!fs_entries[idx].is_dir) return -1;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_entries[i].used && fs_entries[i].parent == idx) {
            cb(fs_entries[i].name, fs_entries[i].size, fs_entries[i].is_dir, cookie);
        }
    }
    return 0;
}
