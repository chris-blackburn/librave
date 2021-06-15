/**
 * Transform
 *
 * Contains information and methods to transform functions. This should be
 * architecture specific.
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/1/1977
 */

#ifndef __TRANSFORM_H_
#define __TRANSFORM_H_

#include "window.h"
#include "function.h"
#include "list.h"

typedef struct transform * transform_t;

/* A set of contiguous, related instructions */
struct instr_set {
	struct list_head l;

	/* The original start and end addresses of the instructions */
	uintptr_t start, end;

	/* Number of instructions and pointer to first instruction (the list is
	 * maintained per-impl, hence the void ptr) */
	size_t nr_instrs;
	void *instrs;
};

/* We need a way to track information about transformed functions. */
struct transformable {
	struct list_head l;

	struct function record;

	/* Instruction sets for prologues and epilogues */
	struct instr_set prologue;
	struct list_head epilogues;
};

transform_t transform_create(void);
void transform_destroy(transform_t self);

int transform_init(transform_t self);
int transform_close(transform_t self);

/* Make sure we can transform the function.
 *
 * TODO: For rave's current state (focusing on just permuting
 * prologue/epilogues), this analysis is very dumb. Originally, I wanted to make
 * it more sophisticated and track the stack delta among other things in the
 * function. However, without referencing additional metadata (e.g. .eh_frame),
 * that analysis is very complicated and storing the additional information
 * about the function doesn't help me much. So, for now, it just records info
 * about prologues and epilogues.
 * */
int transform_add_function(transform_t self, const struct function *record,
	void *bytes);

/* Permute push/pop instructions in the prologue and epilogue of a function */
int transform_permute_all(transform_t self, struct window *text);

#endif /* __TRANSFORM_H_ */

