/**
 * Metadata
 *
 * Metadata can give us a head start in binary analysis/transformation. In order
 * to be more flexible in future design, I created a class which abstracts the
 * source of the metadata. Currently, I'm using dwarf, but in we ever wanted to
 * use a different source, I didn't want rave to be too tightly coupled to dwarf
 * debug info.
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/1/1977
 */

#ifndef __METADATA_H_
#define __METADATA_H_

#include "binary.h"
#include "function.h"

typedef int (*foreach_function_cb)(const struct function *, void *);

typedef struct metadata * metadata_t;
struct metadata_op {
	metadata_t (*create)(void);
	void (*destroy)(metadata_t self);

	int (*init)(metadata_t self, struct binary *binary);
	int (*close)(metadata_t self);

	/* Loop through all functions, once metadata is retrieved, the callback is
	 * called. */
	int (*foreach_function)(metadata_t self, foreach_function_cb cb, void *arg);
};

extern struct metadata_op metadata_dwarf;

#endif /* __METADATA_H_ */

