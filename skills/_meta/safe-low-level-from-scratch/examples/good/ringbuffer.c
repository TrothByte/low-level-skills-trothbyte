// GOOD: the same ring buffer, designed safe from scratch.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct rb {
    unsigned char *buf;
    size_t head, tail;   // size_t indexes — no negative, no overflow for realistic sizes
    size_t cap;          // capacity in bytes
    size_t used;         // invariant: used == (head - tail) mod cap
} rb;

// init: checked allocation, size_t capacity.
int rb_init(rb *r, size_t cap) {
    if (cap == 0 || cap > SIZE_MAX / 2) return -1;   // sanity + overflow guard
    unsigned char *b = (unsigned char *)malloc(cap);
    if (!b) return -1;
    r->buf = b; r->head = r->tail = r->used = 0; r->cap = cap;
    return 0;
}

void rb_free(rb *r) { free(r->buf); r->buf = NULL; r->used = r->cap = 0; }

size_t rb_space(const rb *r) { return r->cap - r->used; }

// write: checks space, bounded copy in at most two segments (no overlap by construction:
// head never crosses tail because we never exceed space).
size_t rb_write(rb *r, const unsigned char *data, size_t n) {
    size_t space = rb_space(r);
    if (n > space) n = space;
    size_t first = r->cap - r->head;
    size_t chunk = n < first ? n : first;
    memcpy(&r->buf[r->head], data, chunk);
    memcpy(&r->buf[0], data + chunk, n - chunk);   // wrap-around segment
    r->head = (r->head + n) % r->cap;
    r->used += n;
    return n;
}

size_t rb_read(rb *r, unsigned char *out, size_t n) {
    size_t avail = r->used;
    if (n > avail) n = avail;
    size_t first = r->cap - r->tail;
    size_t chunk = n < first ? n : first;
    memcpy(out, &r->buf[r->tail], chunk);
    memcpy(out + chunk, &r->buf[0], n - chunk);
    r->tail = (r->tail + n) % r->cap;
    r->used -= n;
    return n;
}
