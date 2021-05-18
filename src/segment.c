#include <string.h>

#include "binary.h"
#include "rave/errno.h"
#include "log.h"

int segment_init(struct segment *self, Elf *elf, GElf_Phdr *header)
{
	self->elf = elf;
	memcpy(&self->header, header, sizeof(*header));

	segment_print(self);
	return RAVE_SUCCESS;
}

uintptr_t segment_address(struct segment *self)
{
	return self->header.p_vaddr;
}

size_t segment_offset(struct segment *self)
{
	return self->header.p_offset;
}

size_t segment_size(struct segment *self)
{
	return self->header.p_filesz;
}

int segment_contains(struct segment *self, uintptr_t address)
{
	return (self->header.p_vaddr <= address &&
		address < self->header.p_vaddr + self->header.p_memsz);
}

#define PRINT_FIELD(N) do { \
	DEBUG("	%-20s 0x%jx\n", #N, (uintmax_t)self->header.p_##N); } while (0)
void segment_print(struct segment *self)
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
