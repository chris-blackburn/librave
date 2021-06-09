/**
 * Errno
 *
 * Error codes
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/1/1977
 */

#ifndef __RAVE_ERRNO_H_
#define __RAVE_ERRNO_H_

/* Error codes */
#define BINARY_CODES \
	X(EELF_INIT, "Could not initialize libelf") \
	X(EELF_MEMORY, "Could not load elf from memory") \
	X(EELF_NOT_SUPPORTED, "Could not initialize libelf") \
	X(EELF_HEADER, "Could not load Elf header") \
	X(EFILE_OPEN, "Could not open file") \
	X(EFILE_STAT, "Could not stat file") \
	X(EMAPPING, "Could not mmap file") \
	X(EFILE_CLOSE, "Could not close file") \
	X(ESECTION_HEADER, "Could not load Elf section header") \
	X(ENO_SECTION, "Could not find requested Elf section") \
	X(ESECTION_DATA, "Could not load Elf section data") \
	X(ENO_SEGMENT, "Could not find requested Elf segment") \
	X(EPROGRAM_HEADER, "Could not load an Elf program header") \
	X(EMAP_FAILED, "MMAP failure") \
	X(ESEG_NOT_LOADABLE, "Tried to load an unloadable Elf segment") \
	X(EDWARF, "Dwarf error - investigate dwarf error codes") \
	X(ETRANSFORM, "transform error")

#define GENERIC_CODES \
	X(EFATAL, "Something bad happened") \
	X(EINVAL, "Invalid parameter") \
	X(ENOMEM, "No memory left")

typedef enum {
	RAVE__SUCCESS = 0,
#define X(code, _) RAVE__##code,
	BINARY_CODES
	GENERIC_CODES
#undef X
} rave_errno_t;

#endif /* __RAVE_ERRNO_H_ */
