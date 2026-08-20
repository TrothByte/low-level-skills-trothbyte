/*
 * stubs.h — self-contained host stubs for Linux sk_buff-shaped code.
 * Models the kernel buffer model as four pointers (head/data/tail/end)
 * over a single allocation, plus a per-struct users refcount and a
 * shared data-area dataref, so skb_reserve / skb_put / skb_push / skb_pull /
 * skb_clone / skb_copy / skb_share_check / skb_free semantics can be
 * exercised with a plain gcc build. No kernel headers required.
 * Not kernel code.
 *
 * Kernel semantics mirrored here:
 *   - skb_put moves tail toward end and NEVER expands the allocation;
 *     a length beyond tailroom is a caller bug (BUG diagnostic).
 *   - skb_push moves data toward head and requires headroom.
 *   - skb_pull advances data and returns the new data pointer.
 *   - skb_clone copies the sk_buff struct and control block (cb) but the
 *     data area is shared: dataref is incremented on both skbs.
 *   - skb_copy duplicates the whole buffer (private data).
 *   - skb_share_check re-clones only when the data area is shared.
 *   - skb_free drops this holder's reference; the data area is freed only
 *     when the last holder goes away. A second free of the same struct is
 *     detected as a double free.
 */
#ifndef SK_BUFF_STUBS_H
#define SK_BUFF_STUBS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* one data allocation; may be referenced by several sk_buff structs */
struct skb_data_area {
	unsigned char *buf;
	unsigned int capacity;
	unsigned int dataref;	/* number of skbs holding this data area */
};

struct sk_buff {
	unsigned char *head;	/* start of the allocation */
	unsigned char *data;	/* start of the current data */
	unsigned char *tail;	/* one past the last data byte */
	unsigned char *end;	/* one past the allocation */
	unsigned int users;	/* refcount of this sk_buff struct */
	unsigned int shared;	/* 1 if the data area is shared */
	int freed;		/* simulator guard: struct freed */
	struct skb_data_area *area;
	unsigned char cb[16];	/* control block (header copy) */
};

static int skb_emu_bugs;

static inline unsigned int skb_headroom_emu(const struct sk_buff *skb)
{
	return (unsigned int)(skb->data - skb->head);
}

static inline unsigned int skb_len_emu(const struct sk_buff *skb)
{
	return (unsigned int)(skb->tail - skb->data);
}

static inline unsigned int skb_tailroom_emu(const struct sk_buff *skb)
{
	return (unsigned int)(skb->end - skb->tail);
}

static inline unsigned char *skb_data_emu(struct sk_buff *skb)
{
	return skb->data;
}

/* alloc_skb: size is the full buffer; head = data = tail = start */
static inline struct sk_buff *skb_alloc_emu(unsigned int size)
{
	struct sk_buff *skb;
	struct skb_data_area *area;

	if (size == 0)
		return NULL;
	area = (struct skb_data_area *)malloc(sizeof(*area));
	if (!area)
		return NULL;
	area->buf = (unsigned char *)malloc(size);
	if (!area->buf) {
		free(area);
		return NULL;
	}
	area->capacity = size;
	area->dataref = 1;
	skb = (struct sk_buff *)calloc(1, sizeof(*skb));
	if (!skb) {
		free(area->buf);
		free(area);
		return NULL;
	}
	skb->head = area->buf;
	skb->data = area->buf;
	skb->tail = area->buf;
	skb->end = area->buf + size;
	skb->users = 1;
	skb->area = area;
	return skb;
}

/* reserve headroom for future headers; call right after allocation */
static inline void skb_reserve_emu(struct sk_buff *skb, unsigned int len)
{
	if (len > (unsigned int)(skb->end - skb->head)) {
		printf("BUG: skb_reserve exceeded buffer size (len %u)\n", len);
		skb_emu_bugs++;
		return;
	}
	skb->data = skb->head + len;
	skb->tail = skb->data;
}

/* grow the data area toward end; caller must guarantee tailroom */
static inline unsigned char *skb_put_emu(struct sk_buff *skb, unsigned int len)
{
	unsigned char *old;

	if (len > skb_tailroom_emu(skb)) {
		printf("BUG: skb_put exceeded tailroom (len %u, tailroom %u)\n",
		       len, skb_tailroom_emu(skb));
		skb_emu_bugs++;
		return NULL;
	}
	old = skb->tail;
	skb->tail += len;
	return old;
}

/* prepend a header: data moves back toward head; needs headroom */
static inline unsigned char *skb_push_emu(struct sk_buff *skb, unsigned int len)
{
	if (len > skb_headroom_emu(skb)) {
		printf("BUG: skb_push exceeded headroom (len %u, headroom %u)\n",
		       len, skb_headroom_emu(skb));
		skb_emu_bugs++;
		return NULL;
	}
	skb->data -= len;
	return skb->data;
}

/* parse: advance data past a consumed header; returns the new data ptr */
static inline unsigned char *skb_pull_emu(struct sk_buff *skb, unsigned int len)
{
	if (len > skb_len_emu(skb)) {
		printf("BUG: skb_pull beyond data (len %u, data %u)\n",
		       len, skb_len_emu(skb));
		skb_emu_bugs++;
		return NULL;
	}
	skb->data += len;
	return skb->data;
}

/* clone: struct + cb copied, data area shared, dataref incremented */
static inline struct sk_buff *skb_clone_emu(struct sk_buff *skb)
{
	struct sk_buff *n;

	if (!skb || skb->freed || !skb->area || skb->area->dataref == 0) {
		printf("BUG: skb_clone of freed skb\n");
		skb_emu_bugs++;
		return NULL;
	}
	n = (struct sk_buff *)calloc(1, sizeof(*n));
	if (!n)
		return NULL;
	n->head = skb->head;
	n->data = skb->data;
	n->tail = skb->tail;
	n->end = skb->end;
	n->users = 1;
	n->shared = 1;
	n->area = skb->area;
	n->area->dataref++;
	skb->shared = 1;
	memcpy(n->cb, skb->cb, sizeof(skb->cb));
	return n;
}

/* copy: whole buffer duplicated, result is private (dataref == 1) */
static inline struct sk_buff *skb_copy_emu(struct sk_buff *skb)
{
	struct sk_buff *n;
	unsigned int cap;
	unsigned int used;

	if (!skb || skb->freed)
		return NULL;
	cap = (unsigned int)(skb->end - skb->head);
	n = skb_alloc_emu(cap);
	if (!n)
		return NULL;
	used = (unsigned int)(skb->tail - skb->head);
	memcpy(n->head, skb->head, used);
	n->data = n->head + (skb->data - skb->head);
	n->tail = n->data + (skb->tail - skb->data);
	memcpy(n->cb, skb->cb, sizeof(skb->cb));
	return n;
}

/* free one reference; the data area dies with its last holder */
static inline void skb_free_emu(struct sk_buff *skb)
{
	if (!skb || skb->freed) {
		printf("BUG: double free of a shared skb\n");
		skb_emu_bugs++;
		return;
	}
	if (skb->users == 0) {
		printf("BUG: freeing skb with zero refcount\n");
		skb_emu_bugs++;
		return;
	}
	skb->users--;
	if (skb->users == 0) {
		skb->area->dataref--;
		if (skb->area->dataref == 0) {
			free(skb->area->buf);
			free(skb->area);
			skb->area = NULL;
		}
		skb->freed = 1;
	}
}

/* optimization: re-clone (and drop this reference) only if data is shared */
static inline struct sk_buff *skb_share_check_emu(struct sk_buff *skb)
{
	struct sk_buff *n;

	if (!skb || skb->freed || !skb->area || skb->area->dataref == 1)
		return skb;
	n = skb_clone_emu(skb);
	if (!n)
		return skb;
	skb_free_emu(skb);
	return n;
}

#endif
