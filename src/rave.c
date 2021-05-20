#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "rave.h"
#include "rave/errno.h"
#include "binary.h"
#include "util.h"
#include "log.h"

struct rave_handle {
	struct binary binary;

	/* We need the metadata stored in the text section */
	struct section text;

	/* True addresses of the code mapping and code addresses */
	uintptr_t region_start, region_end;
	uintptr_t code_start, code_end;

	/* local copy of code pages */
	void *code_pages;
	size_t code_length;
};

/* In order to accurately map code pages, we need both the text section and the
 * segment containing the text section. In an elf segment, the on disk size can
 * be smaller than the in memory size. Additionally, segments are page-aligned,
 * while sections are not guaranteed to be page-aligned. So, in case the text
 * section does not perfectly fill a span of pages, we need to fill the rest of
 * that memory correctly (the segment tells us how to do that). */
static int map_code_pages(rave_handle_t *self, struct section *text,
	struct segment *segment)
{
	uintptr_t region_start, region_end;
	uintptr_t code_start = section_address(text),
		  code_end = code_start + section_size(text);
	void *copy_src, *copy_dst;
	size_t copy_size;

	/* Determine the start address of the region - we want to grab the whole
	 * loadable segment so that we match with the elf (one VMA will be dedicated
	 * to this executable region, which we want to match as we intend to serve
	 * page faults for that VMA). */
	region_start = segment_address(segment);
	region_end = region_start + segment_memsz(segment);

	self->code_length = (region_end - region_start);

	/* There are 3 regions to copy into this memory mapping:
	 * 1. The region before the code
	 * 2. The code
	 * 3. The region after the code */
	self->code_pages = mmap(NULL, self->code_length, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (MAP_FAILED == self->code_pages) {
		ERROR("Could not create anonymous mapping for code pages");
		self->code_pages = NULL;
		return RAVE_EMAPFAILED;
	}

	/* Copy the first region - there should be no holes here... */
	DEBUG("Mapping region before code");
	copy_dst = PTR(self->code_pages);
	copy_src = OFFSET(self->binary.mapping, segment_offset(segment));
	copy_size = code_start - region_start;
	memcpy(copy_dst, copy_src, copy_size);

	/* Copy the code - again, no holes should be here */
	DEBUG("Mapping code region");
	copy_dst = OFFSET(copy_dst, copy_size);
	copy_src = OFFSET(self->binary.mapping, section_offset(text));
	copy_size = section_size(text);
	memcpy(copy_dst, copy_src, copy_size);

	/* Copy the final region - We need to be careful here. It's possible that
	 * the in-memory size of the segment is greater than the file size, so we
	 * can't just blindly copy memory from the file. Since we made an anonymous
	 * mapping, the left over data will be initialized to zero, just like the
	 * rest of the segment as defined by elf. */
	DEBUG("Mapping region after code");
	copy_dst = OFFSET(copy_dst, copy_size);
	copy_src = OFFSET(self->binary.mapping, section_offset(text) +
		section_size(text));
	copy_size = (segment_contains(segment, region_end)) ?
		region_end - code_end :
		segment_filesz(segment) - (code_end - segment_address(segment));
	memcpy(copy_dst, copy_src, copy_size);

	self->region_start = region_start;
	self->region_end = region_end;
	self->code_start = code_start;
	self->code_end = code_end;

	return RAVE_SUCCESS;
}

int rave_init(rave_handle_t *self, const char *filename)
{
	int rc;
	struct segment segment;

	DEBUG("Intializing rave with binary: %s", filename);

	self->code_pages = NULL;
	self->code_length = 0;

	rc = binary_init(&self->binary, filename);
	if (rc != RAVE_SUCCESS) {
		return rc;
	}

	/* let's find the segment containing the code and map it */
	rc = binary_find_section(&self->binary, ".text", &self->text);
	if (rc != RAVE_SUCCESS) {
		FATAL("Couldn't load the text section");
		return rc;
	}

	rc = binary_find_segment(&self->binary, section_address(&self->text),
		&segment);
	if (rc != RAVE_SUCCESS) {
		FATAL("Couldn't load the segment containing the text section");
		return rc;
	}

	/* Now that we've loaded both the text section and it's containing segment,
	 * we can map the pages. */
	rc = map_code_pages(self, &self->text, &segment);
	if (rc != RAVE_SUCCESS) {
		FATAL("Could not map code pages");
		return rc;
	}

	return RAVE_SUCCESS;
}

int rave_close(rave_handle_t *self)
{
	DEBUG("Closing rave handle...");

	if (self->code_pages) {
		if (munmap(self->code_pages, self->code_length) != 0) {
			ERROR("Couldn't unmap file memory");
		} else {
			self->code_pages = NULL;
		}
	}

	return binary_close(&self->binary);
}

size_t rave_handle_size(void)
{
	return sizeof(struct rave_handle);
}

void *rave_handle_fault(rave_handle_t *self, uintptr_t address)
{
	size_t offset;

	/* Check if the faulting address lies within the code section */
	if (!CONTAINS(address, self->region_start, self->region_end))
	{
		return NULL;
	}

	/* return a pointer to the matching page */
	offset = PAGE_DOWN(address) - self->region_start;
	return OFFSET(self->code_pages, offset);
}
