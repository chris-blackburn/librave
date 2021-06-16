#include <stdio.h>
#include <stdlib.h>
#include <rave.h>

int main(int argc, char **argv) {
	rave_handle_t rh = rave_create();
	FILE *dump = NULL;
	void *text;
	size_t length;
	int rc;

	if (argc != 3) {
		fprintf(stderr, "Please provide a binary name and an output file\n");
		goto err;
	}

	dump = fopen(argv[2], "wb");
	if (NULL == dump) {
		fprintf(stderr, "Could not create file for dumping\n");
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

	text = rave_get_text(rh, &length);
	if (NULL == text) {
		fprintf(stderr, "Error getting text\n");
		goto err;
	}


	/* Dump executable segment to output file */
	if (fwrite(text, 1, length, dump) != length) {
		fprintf(stderr, "Error writing to dump file\n");
		goto err;
	}

	fclose(dump);

	rc = rave_close(rh);
	if (rc != 0) {
		fprintf(stderr, "Close failed\n");
		goto err;
	}

	rave_destroy(rh);

	printf("Success!\n");
	return EXIT_SUCCESS;
err:
	if (dump) fclose(dump);
	return EXIT_FAILURE;
}

