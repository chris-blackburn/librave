#include "window.h"
#include "util.h"
#include "rave/errno.h"

int window_init(struct window *self, uintptr_t orig, void *data,
	size_t length)
{
	if (NULL == self) {
		return RAVE__EINVAL;
	}

	self->orig = orig;
	self->data = data;
	self->length = length;

	return RAVE__SUCCESS;
}

void window_relocate(struct window *self, uintptr_t orig)
{
	if (NULL == self) {
		return;
	}

	self->orig = orig;
}

uintptr_t window_orig(struct window *self)
{
	if (NULL == self) {
		return 0;
	}

	return self->orig;
}

void *window_get(struct window *self, size_t *length)
{
	if (NULL == self) {
		return NULL;
	}

	/* The user may not want the length, or we may not have length to give */
	if (NULL != self->data && NULL != length) {
		*length = self->length;
	}

	return self->data;
}

int window_contains(struct window *self, uintptr_t address)
{
	if (NULL == self) {
		return 0;
	}

	return CONTAINS(address, self->orig, OFFSET(self->orig, self->length));
}

void *window_view(struct window *self, uintptr_t address, size_t *length)
{
	if (NULL == self) {
		return NULL;
	}

	if (!window_contains(self, address)) {
		return NULL;
	}

	/* The leftover bytes in this window */
	if (NULL != length) {
		*length = self->length - (address - self->orig);
	}

	return OFFSET(self->data, address - self->orig);
}
