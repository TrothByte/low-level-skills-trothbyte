// BAD: a ring buffer written with common agent mistakes.
#include <stddef.h>
#include <string.h>

struct rb_bad {
    unsigned char *buf;
    int head, tail;   // int for indexes — overflow/negative traps
    int cap;          // int cap — can't hold big buffers; also `malloc(int*1)`
};

int rb_init_bad(struct rb_bad *r, int cap) {
    r->buf = (unsigned char *)malloc((size_t)cap);       // cap from untrusted input
    if (!r->buf) return -1;
    r->head = r->tail = 0;
    r->cap = cap;
    return 0;
}

int rb_write_bad(struct rb_bad *r, const unsigned char *data, int n) {
    for (int i = 0; i <= n; i++) {            // off-by-one: writes data[n]
        r->buf[(r->head + i) % r->cap] = data[i]; // no overflow/space check at all
    }
    r->head = (r->head + n) % r->cap;
    return 0;
}

void rb_read_bad(struct rb_bad *r, unsigned char *out, int n) {
    memcpy(out, &r->buf[r->tail], n);         // ignores capacity, no bounds, possible overlap
}
