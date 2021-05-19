/**
 * Binary
 *
 * Representation of a binary file. This class primarily abstracts libelf. This
 * makes it easier for us to analyze functions in the binary for randomization.
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/1/1977
 */

#ifndef __BINARY_H_
#define __BINARY_H_

#ifdef __cplusplus
extern "C" {
#endif

/* When using libelf, we should work with the class independant API. This way,
 * this program could be more easily extended to 32-bit if such support was
 * ever desired. Of course, for now, we limit usage to 64-bit binaries. */
#include <gelf.h>
#include <libelf.h>
#include <stdint.h>

/* Elf sections */
struct section {
	Elf *elf;
	GElf_Shdr header;
	Elf_Data *data;

	/* This should be an offset into the memory loaded elf (i.e. not a pointer
	 * which needs to be freed) */
	const char *name;
};

int section_init(struct section *self, Elf *elf, GElf_Shdr *header,
	const char *name, Elf_Scn *scn);

int section_address(const struct section *self);
size_t section_offset(const struct section *self);
size_t section_size(const struct section *self);

void section_print(const struct section *self);

/* Elf segments */
struct segment {
	Elf *elf;
	GElf_Phdr header;
};

int segment_init(struct segment *self, Elf *elf, GElf_Phdr *header);

uintptr_t segment_address(const struct segment *self);
size_t segment_offset(const struct segment *self);
size_t segment_filesz(const struct segment *self);
size_t segment_memsz(const struct segment *self);

int segment_contains(const struct segment *self, uintptr_t address);

void segment_print(const struct segment *self);

/* Elf binary */
struct binary {
    Elf *elf;
    GElf_Ehdr header;

	/* File */
	void *mapping;
	int file_size;
};

int binary_init(struct binary *self, const char *filename);
int binary_close(struct binary *self);

int binary_find_section(const struct binary *self, const char *target,
	struct section *section);
int binary_find_segment(const struct binary *self, uintptr_t address,
	struct segment *segment);

void binary_print(const struct binary *self);

#ifdef __cplusplus
}
#endif

#endif /* __BINARY_H_ */
