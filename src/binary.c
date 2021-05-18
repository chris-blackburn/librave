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

static int load_section(struct binary *self, const char *target,
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

static int init_sections(struct binary *self)
{
	int rc;

	/* load each section */
	rc = load_section(self, ".text", &self->text);
	if (rc != 0) {
		FATAL("Couldn't load the text section");
		return rc;
	}

	// TODO: load dwarf debugging sections

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

	/* Load the sections we want */
	rc = init_sections(self);
	if (rc != 0) {
		return rc;
	}

	binary_print(self);
	return RAVE_SUCCESS;
}

#if 0
// TODO: will use this type of code for CRIU integration (for userfaultfd)
__attribute__((unused))
ReturnCode Binary::loadCodePages()
{
	GElf_Phdr phdr;
	Segment seg;
	uintptr_t regionStart, regionEnd;
	size_t codeStart = text.address(), codeEnd = text.address() + text.size();

	/* Load in all pages containing code. The segment in which the text section
	 * resides in may have extra data before and after the text section. So, we
	 * need to be sure to grab all that as well. The mutable region is still
	 * only what is contained in the code section though. */
	for (size_t i = 0; i < header.e_phnum; i++) {
		if (gelf_getphdr(elf, i, &phdr) != &phdr) {
			return ReturnCode::ElfProgramHeader;
		}

		new(&seg) Segment(elf, phdr);

		/* Check if it's the segment containing the code section. */
		if (seg.contains(codeStart)) {
			DEBUG("Found the segment containing the text segment");

			/* We only want pages with code, so we need to grab the correct
			 * starting address of those pages. */
			regionStart = std::max<uintptr_t>(PAGE_DOWN(codeStart),
				seg.address());
			regionEnd = PAGE_UP(codeEnd);

			DEBUG("Code pages start<->end: 0x%jx<->0x%jx", regionStart, regionEnd);
			DEBUG("Code start<->end: 0x%jx<->0x%jx", codeStart, codeEnd);

			/* It's possible that for each region (except the first) that all
			 * the data may not be located in the file, in which case we have to
			 * zero out the rest of the data */
			codePagesSize = regionEnd - regionStart;

			/* Calloc and copy regions (calloc zeros mem for us) */
			codePages.reset((unsigned char*)std::calloc(1, codePagesSize));
			if (nullptr == codePages.get()) {
				return ReturnCode::BadAlloc;
			}

			// TODO: if there is a hole here, then we have a bad elf, we should
			// throw an error
			memcpy(codePages.get(),
				OFFSET(fmap, seg.offset()),
				codeStart - regionStart);

			/* This is where we might start having memsz > filesz, so we need to
			 * be careful about how much we copy from the file. */
			{
				void *dest, *src;
				size_t size;

				/* Code section */
				dest = OFFSET(codePages.get(), (codeStart - regionStart));
				src = OFFSET(fmap, text.offset());

				if (seg.fileSize() - codeStart < text.size()) {
					size = seg.fileSize() - codeStart;
				} else {
					size = text.size();
				}

				memcpy(dest, src, size);

				/* region after code section */
				dest = OFFSET(codePages.get(), (codeStart - regionStart) + text.size());
				src = OFFSET(fmap, text.offset() + text.size());

				if (seg.fileSize() - codeEnd < regionEnd - codeEnd) {
					size = seg.fileSize() - codeEnd;
				} else {
					size = regionEnd - codeEnd;
				}

				memcpy(dest, src, size);
			}

			DEBUG("Code pages loaded %p:%lu", codePages.get(), codePagesSize / PAGESZ);

			return ReturnCode::Success;
		}
	}

	return ReturnCode::ElfNoTextSegment;
}
#endif

int binary_close(struct binary *self)
{
	if (NULL == self) {
		return RAVE_SUCCESS;
	}

	if (self->elf) {
		elf_end(self->elf);
	}

	if (self->mapping) {
		if (munmap(self->mapping, self->file_size) != 0) {
			ERROR("Couldn't unmap file memory");
		}
	}

	DEBUG("Binary unloaded");
	return RAVE_SUCCESS;
}

#define PRINT_FIELD(N) do { \
	printf("	%-20s 0x%jx\n", #N, (uintmax_t)self->header.e_##N); } while (0)
void binary_print(struct binary *self)
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
