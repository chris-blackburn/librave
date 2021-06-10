/**
 * Window
 *
 * A more convenient way of messing with memory where we have a local data
 * and a true address. Given a true address (e.g. the virtual address of a
 * loaded segment vs the local address), operate on memory regions.
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/1/1977
 */

#ifndef __WINDOW_H_
#define __WINDOW_H_

#include <stddef.h>
#include <inttypes.h>

struct window {
	/* The original, intended, or loaded address of this region */
	uintptr_t orig;

	/* local data and the size of this data in bytes */
	void *data;
	size_t length;
};

int window_init(struct window *self, uintptr_t orig, void *data,
	size_t length);

void window_relocate(struct window *self, uintptr_t orig);

uintptr_t window_orig(struct window *self);

/* Get the underlying data and length */
void *window_get(struct window *self, size_t *length);

/* Checks if a window contains a real address */
int window_contains(struct window *self, uintptr_t address);

/* Get a pointer to a real address backed by the window. Also gives back the
 * amount of data remaining in the window */
void *window_view(struct window *self, uintptr_t address, size_t *length);

#endif /* __WINDOW_H_ */

