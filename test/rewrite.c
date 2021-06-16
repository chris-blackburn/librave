#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rave.h>

int main(int argc, char **argv) {
	rave_handle_t rh = rave_create();
	FILE *dump = NULL;
	void *text, *_text;
	size_t length, offset;
	int rc;

	if (argc != 2) {
		fprintf(stderr, "Please provide a binary to rewrite\n");
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

	_text = rave_get_text(rh, &length);
	if (NULL == _text) {
		fprintf(stderr, "Error getting text\n");
		goto err;
	}

	text = malloc(length);
	if (NULL == text) {
		fprintf(stderr, "no mem\n");
		goto err;
	}
	memcpy(text, _text, length);

	offset = rave_get_text_offset(rh);
	if (offset == 0) {
		fprintf(stderr, "Rave gave bad offset\n");
		goto err;
	}

	rc = rave_close(rh);
	if (rc != 0) {
		fprintf(stderr, "Close failed\n");
		goto err;
	}

	rave_destroy(rh);

	dump = fopen(argv[1], "r+b");
	if (NULL == dump) {
		fprintf(stderr, "Could not create file for dumping\n");
		goto err;
	}

	rc = fseek(dump, offset, SEEK_SET);
	if (rc != 0) {
		fprintf(stderr, "Failed to fseek to text section\n");
		goto err;
	}

	if (fwrite(text, 1, length, dump) != length) {
		fprintf(stderr, "Error writing to dump file\n");
		goto err;
	}

	fclose(dump);

	printf("Success!\n");
	return EXIT_SUCCESS;
err:
	if (dump) fclose(dump);
	if (text) free(text);
	return EXIT_FAILURE;
}

