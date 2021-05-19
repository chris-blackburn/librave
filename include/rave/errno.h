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
	X(EELFINIT, "Could not initialize libelf") \
	X(EELFMEMORY, "Could not load elf from memory") \
	X(EELFNOTSUPPORTED, "Could not initialize libelf") \
	X(EELFHEADER, "Could not load Elf header") \
	X(EFILEOPEN, "Could not open file") \
	X(EFILESTAT, "Could not stat file") \
	X(EMAPPING, "Could not mmap file") \
	X(EFILECLOSE, "Could not close file") \
	X(ESECTIONHEADER, "Could not load Elf section header") \
	X(ENOSECTION, "Could not find requested Elf section") \
	X(ESECTIONDATA, "Could not load Elf section data") \
	X(ENOSEGMENT, "Could not find requested Elf segment") \
	X(EPROGRAMHEADER, "Could not load an Elf program header") \
	X(EMAPFAILED, "MMAP failure")

#define GENERIC_CODES \
	X(FatalError, "Something bad happened")

typedef enum {
	RAVE_SUCCESS = 0,
#define X(code, _) RAVE_##code,
	BINARY_CODES
	GENERIC_CODES
#undef X
} rave_errno_t;

#endif /* __RAVE_ERRNO_H_ */
