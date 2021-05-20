#include <string.h>

#include "binary.h"
#include "rave/errno.h"
#include "log.h"

int segment_init(struct segment *self, Elf *elf, GElf_Phdr *header)
{
	self->elf = elf;
	memcpy(&self->header, header, sizeof(*header));

	return RAVE_SUCCESS;
}

uintptr_t segment_vaddr(const struct segment *self)
{
	return self->header.p_vaddr;
}

size_t segment_offset(const struct segment *self)
{
	return self->header.p_offset;
}

size_t segment_filesz(const struct segment *self)
{
	return self->header.p_filesz;
}

size_t segment_memsz(const struct segment *self)
{
	return self->header.p_memsz;
}

int segment_loadable(const struct segment *self)
{
	return !!(self->header.p_type & PT_LOAD);
}

int segment_contains(const struct segment *self, uintptr_t address)
{
	return (self->header.p_vaddr <= address &&
		address < self->header.p_vaddr + self->header.p_memsz);
}

#define PRINT_FIELD(N) do { \
	printf("	%-20s 0x%jx\n", #N, (uintmax_t)self->header.p_##N); } while (0)
void segment_print(const struct segment *self)
{
	if (NULL == self->elf) {
		printf("Emtpy segment\n");
		return;
	}

	printf("Segment\n");

	PRINT_FIELD(type);
	PRINT_FIELD(flags);
	PRINT_FIELD(offset);
	PRINT_FIELD(vaddr);
	PRINT_FIELD(paddr);
	PRINT_FIELD(filesz);
	PRINT_FIELD(memsz);
	PRINT_FIELD(align);
}
#undef PRINT_FIELD
