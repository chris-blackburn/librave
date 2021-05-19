#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#include "binary.h"
#include "rave/errno.h"
#include "log.h"

// TODO: Move to arch specific code
static int check_arch(uint16_t arch)
{
	return (arch == EM_X86_64);
}

static int init_libelf()
{
	static int initialized = 0;

	/* Init once */
	if (!initialized) {
		initialized = 1;

		if (elf_version(EV_CURRENT) == EV_NONE) {
			return RAVE_EELFINIT;
		}
	}

	return RAVE_SUCCESS;
}

/* Create a memory mapping of the binary */
static int map_file(struct binary *self, const char *filename)
{
	int fd;
	struct stat statbuf;

	if ((fd = open(filename, O_RDONLY)) == -1) {
		FATAL("Could not open file %s", filename);
		return RAVE_EFILEOPEN;
	}

	if ((fstat(fd, &statbuf)) == -1) {
		FATAL("Could not stat file %s", filename);
		return RAVE_EFILESTAT;
	}

	self->file_size = statbuf.st_size;

	self->mapping = mmap(NULL, self->file_size, PROT_READ, MAP_SHARED, fd, 0);
	if (self->mapping == NULL) {
		FATAL("Could not mmap file %s", filename);
		return RAVE_EMAPPING;
	}

	if (close(fd) == -1) {
		ERROR("Could not close file %s", filename);
		return RAVE_EFILECLOSE;
	}

	return RAVE_SUCCESS;
}

int binary_init(struct binary *self, const char *filename)
{
	int rc;
	DEBUG("Initializing binary from file: %s", filename);

	self->mapping = NULL;
	self->file_size = 0;
	self->elf = NULL;

	rc = init_libelf();
	if (rc != 0) {
		return rc;
	}

	rc = map_file(self, filename);
	if (rc != 0) {
		return rc;
	}

	/* Init the elf file */
	self->elf = elf_memory((char *)self->mapping, self->file_size);
	if (self->elf == NULL) {
		return RAVE_EELFMEMORY;
	}

	/* Make sure we have a valid elf object */
	/* We need to make sure this is a valid elf file and that it is 64-bit. */
	// TODO: verify endianness??
	if (elf_kind(self->elf) != ELF_K_ELF &&
		gelf_getclass(self->elf) != ELFCLASS64)
	{
		FATAL("Only 64-bit elfs are supported at this time");
		return RAVE_EELFNOTSUPPORTED;
	}

	/* Now, we can get the headers and get the target arch */
	if (gelf_getehdr(self->elf, &self->header) != &self->header) {
		return RAVE_EELFHEADER;
	}

	/* The elf must be an executable. */
	if (self->header.e_type != ET_EXEC) {
		FATAL("rave only supports executable elfs");
		return RAVE_EELFNOTSUPPORTED;
	}

	/* Make sure we support this arch */
	if (!check_arch(self->header.e_machine)) {
		return RAVE_EELFNOTSUPPORTED;
	}

	return RAVE_SUCCESS;
}

int binary_close(struct binary *self)
{
	if (NULL == self) {
		return RAVE_SUCCESS;
	}

	if (self->elf) {
		elf_end(self->elf);
		self->elf = NULL;
	}

	if (self->mapping) {
		if (munmap(self->mapping, self->file_size) != 0) {
			ERROR("Couldn't unmap file memory");
		} else {
			self->mapping = NULL;
		}
	}

	DEBUG("Binary unloaded");
	return RAVE_SUCCESS;
}

int binary_find_section(const struct binary *self, const char *target,
	struct section *section)
{
	GElf_Shdr shdr;
	Elf_Scn *scn = NULL;
	const char *iter;

	for (size_t i = 1; (scn = elf_nextscn(self->elf, scn)); i++) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			return RAVE_ESECTIONHEADER;
		}

		iter = elf_strptr(self->elf, self->header.e_shstrndx, shdr.sh_name);
		if (NULL == iter) {
			ERROR("Could not load section name from section header for target %s",
				target);
			continue;
		}

		if (strncmp(target, iter, strlen(target)) == 0) {
			return section_init(section, self->elf, &shdr, iter, scn);
		}
	}

	ERROR("Section named %s not found", target);
	return RAVE_ENOSECTION;
}

int binary_find_segment(const struct binary *self, uintptr_t address,
	struct segment *segment)
{
	GElf_Phdr phdr;

	for (size_t i = 0; i < self->header.e_phnum; i++) {
		if (gelf_getphdr(self->elf, i, &phdr) != &phdr) {
			return RAVE_EPROGRAMHEADER;
		}

		if (segment_init(segment, self->elf, &phdr) != RAVE_SUCCESS) {
			WARN("Failed to initialize segment... continuing search");
			continue;
		}

		/* Check if it's the segment containing the target address */
		if (segment_contains(segment, address)) {
			return RAVE_SUCCESS;
		}
	}

	return RAVE_ENOSEGMENT;
}

#define PRINT_FIELD(N) do { \
	printf("	%-20s 0x%jx\n", #N, (uintmax_t)self->header.e_##N); } while (0)
void binary_print(const struct binary *self)
{
	printf("ELF Headers:\n");
	PRINT_FIELD(type);
	PRINT_FIELD(machine);
	PRINT_FIELD(version);
	PRINT_FIELD(entry);
	PRINT_FIELD(phoff);
	PRINT_FIELD(shoff);
	PRINT_FIELD(flags);
	PRINT_FIELD(ehsize);
	PRINT_FIELD(phentsize);
	PRINT_FIELD(phnum);
	PRINT_FIELD(shentsize);
	PRINT_FIELD(shnum);
	PRINT_FIELD(shstrndx);
}
#undef PRINT_FIELD
