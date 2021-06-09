#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <inttypes.h>

#include "rave.h"
#include "rave/errno.h"
#include "binary.h"
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

	/* The memory mapping containing code pages */
	// TODO: re-org because this is confusing
	struct {
		/* Where in memory this segment will be located */
		uintptr_t vaddr;

		/* offsets into the local mapping telling us where the text section is
		 * located inside this loaded segment */
		size_t start;
		uintptr_t end;

		/* The local mapping and the size in bytes (the size also corresponds to
		 * the memory size of the loaded, containing segment) */
		void *mapping;
		size_t length;
	} code;
};

/* Callback used when iterating through function metadata. Returns success
 * unless there is a fatal error (e.g. nomem) */
static int process_function(const struct function *function, void *arg)
{
	struct rave_handle *self = (struct rave_handle *)arg;
	void *pf;
	int rc = 0;

	DEBUG("Processing function @ %"PRIxPTR", size = %zu", function->lo,
		function->hi - function->lo);

	/* We have to make sure the function addresses are virtually contained by
	 * the text section */
	{
		uintptr_t text;
		size_t textsz;

		text = self->code.vaddr + self->code.start;
		textsz = self->code.end - self->code.start;

		/* Find the function in the mapped code region */
		rc |= !CONTAINS(function->lo, text, text + textsz);
		rc |= !CONTAINS(function->hi, text, text + textsz);
		if (rc) {
			WARN("Can't modify function - not in text section");
			return RAVE__SUCCESS;
		}
	}

	/* Get a pointer to the locally-loaded target function */
	pf = OFFSET(self->code.mapping, function->lo - self->code.vaddr);

	/* Let the transformer verify the function */
	rc = transform_is_safe(NULL, function->lo, pf, function->hi - function->lo);
	if (rc != RAVE__SUCCESS) {
		WARN("Could not verify function @ %"PRIxPTR, function->lo);
		return RAVE__SUCCESS;
	}

	/* Transform the function */

	return RAVE__SUCCESS;
}

struct rave_handle * rave_create(void)
{
	return rave_malloc(sizeof(struct rave_handle));
}

void rave_destroy(struct rave_handle *self)
{
	if (NULL != self) {
		mop->destroy(self->metadata);
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

	/* Since we are not faithfully mapping segments, we are mapping this as r/w
	 * as we intend to modify code. */
	mapping = mmap(NULL, length, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (MAP_FAILED == mapping) {
		ERROR("Could not create anonymous mapping for code pages");
		return RAVE__EMAP_FAILED;
	}

	/* Copy the segment data from the file, the rest will be zero-filled */
	DEBUG("Mapping segment");
	copy_dst = mapping;
	copy_src = OFFSET(self->binary.mapping, segment_offset(segment));
	copy_size = segment_filesz(segment);
	memcpy(copy_dst, copy_src, copy_size);

	self->code.vaddr = segment_vaddr(segment);
	self->code.start = section_offset(text) - segment_offset(segment);
	self->code.end = self->code.start + section_size(text);
	self->code.mapping = mapping;
	self->code.length = length;

	DEBUG("Locally loaded segment intended for: %"PRIxPTR" -> %"PRIxPTR
		" (%zu pages)", self->code.vaddr, self->code.vaddr + self->code.length,
		self->code.length / PAGESZ);

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

	rc = transform_init(NULL);
	if (rc != RAVE__SUCCESS) {
		goto err;
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

	/* Load metadata about the binary*/
	self->metadata = mop->create();
	if (NULL == self->metadata) {
		FATAL("No memory for metadata");
		return RAVE__ENOMEM;
	}

	rc = mop->init(self->metadata, &self->binary);
	if (rc != RAVE__SUCCESS) {
		FATAL("Could not initialize binary metadata");
		return rc;
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
	int rc = 0;

	if (NULL == self) {
		return 0;
	}

	DEBUG("Closing rave handle...");

	if (self->code.mapping) {
		if (munmap(self->code.mapping, self->code.length) != 0) {
			ERROR("Couldn't unmap file memory");
		} else {
			self->code.mapping = NULL;
			self->code.length = 0;
		}
	}

	rc |= mop->close(self->metadata);
	rc |= binary_close(&self->binary);
	return rc;
}

void *rave_handle_fault(struct rave_handle *self, uintptr_t address)
{
	if (NULL == self) {
		return NULL;
	}

	/* Check if the faulting address lies within the code pages */
	if (CONTAINS(address, self->code.vaddr,
		self->code.vaddr + self->code.length))
	{
		return OFFSET(self->code.mapping, PAGE_DOWN(address) -
			self->code.vaddr);
	}

	return NULL;
}

void *rave_get_code(struct rave_handle *self, size_t *length)
{
	if (NULL == self) {
		return NULL;
	}

	if (self->code.mapping && self->code.length) {
		*length = self->code.length;
		return self->code.mapping;
	}

	return NULL;
}
