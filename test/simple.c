#include <stdio.h>
#include <stdlib.h>
#include <rave.h>

int main(int argc, char **argv) {
	rave_handle_t *rh = malloc(rave_handle_size());
	int rc;

	if (argc != 2) {
		fprintf(stderr, "Please provide a binary name\n");
		goto err;
	}

	rc = rave_init(rh, argv[1]);
	if (rc != 0) {
		fprintf(stderr, "Init failed\n");
		goto err;
	}

	rc = rave_close(rh);
	if (rc != 0) {
		fprintf(stderr, "Close failed\n");
		goto err;
	}

	free(rh);

	printf("Success!\n");
	return EXIT_SUCCESS;
err:
	return EXIT_FAILURE;
}

