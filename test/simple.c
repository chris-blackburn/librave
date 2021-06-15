#include <stdio.h>
#include <stdlib.h>
#include <rave.h>

int main(int argc, char **argv) {
	rave_handle_t rh = rave_create();
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

	rc = rave_randomize(rh);
	if (rc != 0) {
		fprintf(stderr, "randomization failed\n");
		goto err;
	}

	rc = rave_close(rh);
	if (rc != 0) {
		fprintf(stderr, "Close failed\n");
		goto err;
	}

	rave_destroy(rh);

	printf("Success!\n");
	return EXIT_SUCCESS;
err:
	return EXIT_FAILURE;
}

