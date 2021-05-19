#include <string.h>

#include "binary.h"
#include "rave/errno.h"
#include "log.h"

int section_init(struct section *self, Elf *elf, GElf_Shdr *header,
	const char *name, Elf_Scn *scn)
{
	self->elf = elf;
	self->name = name;
	memcpy(&self->header, header, sizeof(*header));

	/* Get a pointer to the data in the section */
	self->data = elf_getdata(scn, NULL);
	if (NULL == self->data) {
		FATAL("Could not load section data for %s", name);
		return RAVE_ESECTIONDATA;
	}

	return RAVE_SUCCESS;
}

int section_address(const struct section *self)
{
	return self->header.sh_addr;
}

size_t section_offset(const struct section *self)
{
	return self->header.sh_offset;
}

size_t section_size(const struct section *self)
{
	return self->header.sh_size;
}

#define PRINT_FIELD(N) do { \
	printf("	%-20s 0x%jx\n", #N, (uintmax_t)self->header.sh_##N); } while (0)
void section_print(const struct section *self)
{
	if (NULL == self->elf) {
		printf("Emtpy section\n");
		return;
	}

	printf("Section %s\n", self->name);

	PRINT_FIELD(type);
	PRINT_FIELD(flags);
	PRINT_FIELD(addr);
	PRINT_FIELD(offset);
	PRINT_FIELD(size);
	PRINT_FIELD(link);
	PRINT_FIELD(info);
	PRINT_FIELD(addralign);
	PRINT_FIELD(entsize);
}
#undef PRINT_FIELD
