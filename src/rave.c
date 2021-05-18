#include <stdio.h>

#include "rave.h"
#include "binary.h"

struct rave_handle {
	struct binary binary;

	/* Mapping for randomized code and stacks */
	void *code;
	size_t code_size;
};

int rave_init(rave_handle_t *self, const char *filename)
{
	int rc;

	printf("Intializing rave with binary: %s\n", filename);

	self->code = NULL;

	rc = binary_init(&self->binary, filename);
	if (!rc) {
		return rc;
	}

	/* let's find the segment containing the code */
	// TODO:

	return 0;
}

int rave_close(rave_handle_t *self)
{
	printf("Closing rave handle...\n");

	return binary_close(&self->binary);
}

size_t rave_handle_size(void)
{
	return sizeof(struct rave_handle);
}
