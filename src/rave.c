#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <inttypes.h>

#include "rave.h"
#include "rave/errno.h"
#include "binary.h"
#include "window.h"
#include "function.h"
#include "metadata.h"
#include "transform.h"
#include "memory.h"
#include "util.h"
#include "log.h"

/* Use dwarf metadata */
static struct metadata_op *mop = &metadata_dwarf;

struct rave_handle {
	struct binary binary;

	metadata_t metadata;
	transform_t transform;

	/* The memory mapping containing code pages */
	struct {
		/* The entire loadable code segment. I load the whole segment (and not
		 * just the text section) because it's just easier to serve page faults
		 * when I don't have fragmented regions of memory. */
		struct window segment;

		/* A convenience window for looking specifically at the text section.
		 * This is backed by the same memory used for the segment:
		 *
		 * +---------+------+-----+----------------+
		 * |   seg   | text | seg |      zero      |
		 * +---------+------+-----+----------------+
		 *
		 * */
		struct window text;
	} code;

	/* The executable segment could have been loaded somewhere else in memory,
	 * so we need an offset to reflect that */
	size_t reloc_offset;
};

/* Callback used when iterating through function metadata. Returns success
 * unless there is a fatal error (e.g. nomem) */
static int process_function(const struct function *function, void *arg)
{
	struct rave_handle *self = (struct rave_handle *)arg;
	void *bytes;
	int rc;

	DEBUG("Processing function @ 0x%"PRIxPTR", size = %zu", function->addr,
		function->len);

	/* We have to make sure the function addresses are virtually contained by
	 * the text section */
	rc = 0;
	rc |= !window_contains(&self->code.text, function->addr);
	rc |= !window_contains(&self->code.text, function->addr + function->len);
	if (rc) {
		WARN("Can't modify function - not in text section");
		return RAVE__SUCCESS;
	}

	/* Get a pointer to the locally-loaded target function */
	bytes = window_view(&self->code.text, function->addr, NULL);

	/* Let the transformer verify the function */
	rc = transform_add_function(self->transform, function, bytes);
	if (rc != RAVE__SUCCESS) {
		WARN("non-randomizable function @ 0x%"PRIxPTR, function->addr);
		return RAVE__SUCCESS;
	}

	return RAVE__SUCCESS;
}

struct rave_handle * rave_create(void)
{
	return rave_malloc(sizeof(struct rave_handle));
}

void rave_destroy(struct rave_handle *self)
{
	if (NULL != self) {
		rave_free(self);
	}
}

/* In order to accurately map code pages, we need the segment containing the
 * text section. In an elf segment, the on disk size can be smaller than the in
 * memory size. */
static int map_code_pages(struct rave_handle *self, struct section *text,
	struct segment *segment)
{
	void *copy_src, *copy_dst;
	size_t copy_size;
	void *mapping;
	size_t length;

	/* It's probably the wrong segment if it's not loadable... */
	if (!segment_loadable(segment)) {
		ERROR("Cannot map a non-loadable segment");
		return RAVE__ESEG_NOT_LOADABLE;
	}

	length = PAGE_UP(segment_memsz(segment));

	/* Map a mock region for the executable segment which we can modify */
	mapping = mmap(NULL, length, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (MAP_FAILED == mapping) {
		FATAL("Could not map code segment");
		return RAVE__EMAP_FAILED;
	}

	/* Copy the segment from the binary file */
	copy_dst = mapping;
	copy_src = OFFSET(self->binary.mapping, segment_offset(segment));
	copy_size = segment_filesz(segment);
	memcpy(copy_dst, copy_src, copy_size);

	/* The full segment window */
	window_init(&self->code.segment,
		segment_vaddr(segment),
		mapping,
		length);

	/* Convenience window to access text section directly */
	window_init(&self->code.text,
		section_address(text),
		OFFSET(mapping, section_offset(text) - segment_offset(segment)),
		section_size(text));

	DEBUG("Locally loaded segment intended for: 0x%"PRIxPTR" (%zu pages)",
		segment_vaddr(segment), length / PAGESZ);

	return RAVE__SUCCESS;
}

int rave_init(struct rave_handle *self, const char *filename)
{
	int rc;
	struct section text;
	struct segment segment;

	DEBUG("Intializing rave with binary: %s", filename);

	if (NULL == self) {
		return 0;
	}

	self->metadata = mop->create();
	if (NULL == self->metadata) {
		FATAL("No memory for metadata");
		return RAVE__ENOMEM;
	}

	self->transform = transform_create();
	if (NULL == self->transform) {
		FATAL("No memory for transform");
		return RAVE__ENOMEM;
	}

	rc = binary_init(&self->binary, filename);
	if (rc != RAVE__SUCCESS) {
		goto err;
	}

	/* let's find the segment containing the code and map it */
	rc = binary_find_section(&self->binary, ".text", &text);
	if (rc != RAVE__SUCCESS) {
		FATAL("Couldn't load the text section");
		goto err;
	}

	rc = binary_find_segment(&self->binary, section_address(&text),
		&segment);
	if (rc != RAVE__SUCCESS) {
		FATAL("Couldn't load the segment containing the text section");
		return rc;
	}

	rc = mop->init(self->metadata, &self->binary);
	if (rc != RAVE__SUCCESS) {
		FATAL("Could not initialize binary metadata");
		return rc;
	}

	rc = transform_init(self->transform);
	if (rc != RAVE__SUCCESS) {
		goto err;
	}

	/* Now that we've loaded both the text section and it's containing segment,
	 * we can map the pages. */
	rc = map_code_pages(self, &text, &segment);
	if (rc != RAVE__SUCCESS) {
		FATAL("Could not map code pages");
		return rc;
	}

	/* With both the code and metadata loaded, we can now analyze the binary to
	 * get, prune, and transform functions */
	rc = mop->foreach_function(self->metadata, process_function, self);
	if (rc != RAVE__SUCCESS) {
		FATAL("An error occured while processing metadata");
		return rc;
	}

	return RAVE__SUCCESS;
err:
	/* Make sure to close anything that has been initialized if we didn't make
	 * it all the way through */
	rave_close(self);
	return rc;
}

int rave_close(struct rave_handle *self)
{
	void *code_mapping;
	size_t code_length;
	int rc = 0;

	if (NULL == self) {
		return 0;
	}

	DEBUG("Closing rave handle...");

	code_mapping = window_get(&self->code.segment, &code_length);
	if (code_mapping) {
		if (munmap(code_mapping, code_length) != 0) {
			WARN("Couldn't unmap code mapping");
		}
	}

	rc |= mop->close(self->metadata);
	mop->destroy(self->metadata);
	rc |= binary_close(&self->binary);
	rc |= transform_close(self->transform);
	transform_destroy(self->transform);
	return rc;
}

/* trigger a randomization */
int rave_randomize(rave_handle_t self)
{
	int rc;

	if (NULL == self) {
		return RAVE__EINVAL;
	}

	rc = transform_permute_all(self->transform, &self->code.text);
	return rc;
}

int rave_relocate(rave_handle_t self, uintptr_t address)
{
	if (NULL == self) {
		return RAVE__EINVAL;
	}

	/* Offset from the original address */
	self->reloc_offset = window_orig(&self->code.segment) - address;

	return RAVE__SUCCESS;
}

void *rave_handle_fault(struct rave_handle *self, uintptr_t address)
{
	void *page;
	size_t length;

	if (NULL == self) {
		return NULL;
	}

	address = PAGE_DOWN(address) + self->reloc_offset;

	if (!window_contains(&self->code.segment, address)) {
		return NULL;
	}

	/* If for some reason, the leftover length is less than a page, then we have
	 * a problem */
	page = window_view(&self->code.segment, address, &length);
	if (length < PAGESZ) {
		ERROR("Not enough memory in code segment for a full page");
		return NULL;
	} else if (length % PAGESZ) {
		WARN("Code segment might be missing data (length mismatch)");
	}

	return page;
}

void *rave_get_code(struct rave_handle *self, size_t *length)
{
	uintptr_t vaddr;

	if (NULL == self) {
		return NULL;
	}

	vaddr = window_orig(&self->code.segment);
	return window_view(&self->code.segment, vaddr, length);
}
