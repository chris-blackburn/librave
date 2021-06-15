#include <string.h>
#include <inttypes.h>

#define X86_64
#define LINUX
#include <dr_api.h>

#include "transform.h"
#include "rave/errno.h"
#include "util.h"
#include "log.h"

struct transform {

};

int transform_init(UNUSED struct transform *self)
{
	if (!dr_set_isa_mode(GLOBAL_DCONTEXT, DR_ISA_AMD64, NULL)) {
		return RAVE__ETRANSFORM;
	}

	return RAVE__SUCCESS;
}

#if 0 // Too aggressive, simpler analysis will do for pro/epilogue permutation
#define STACK_DELTA_INIT (-8)
/* returns the stack delta resulting from a single instruction */
static void update_stack_delta(int *stack_delta, instr_t *instr)
{
	int opcode;

	/* If we don't write to the stack pointer nothing to worry about here */
	if (!instr_writes_to_reg(instr, DR_REG_XSP, DR_QUERY_INCLUDE_ALL)) {
		return;
	}

	/* We don't need to track call's disturbance of the stack pointer since it's
	 * really just switching to a new frame. */
	if (instr_is_call(instr)) {
		return;
	}

	/* Returns always pop the return address off the stack */
	if (instr_is_return(instr)) {
		*stack_delta += 8;
		return;
	}

	/* For any other instructions, we have to be more precarious */
	opcode = instr_get_opcode(instr);
	switch (opcode) {
	/* push and pop size determined through operand size */
	case OP_push:
	case OP_push_imm:
		*stack_delta -=
			opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, 0)));
		return;
	case OP_pop:
		*stack_delta +=
			opnd_size_in_bytes(opnd_get_size(instr_get_dst(instr, 0)));
		return;
	case OP_pushf:
		*stack_delta -= 8;
		return;
	case OP_popf:
		*stack_delta += 8;
		return;
	case OP_leave:
		/* This frees the whole frame except for the return address */
		*stack_delta = -8;
		return;
	// TODO: There are other instructions that just clean up the whole stack.
	// e.g. sometimes you'll see lea offset(%rbp),%rsp which just plops the
	// stack pointer back to some other position. That delta becomes very hard
	// to track...
	case OP_add:
	case OP_sub:
	case OP_lea:
		WARN("Caught known stack delta instruction, but not paying attention");
	default:
		WARN("Stack delta modified by uncaught instruction dr op = %d", opcode);
		return;
	}
}
#endif

/* Test for instructions in the prologue. Should look like:
 *
 * push %rbp
 * mov %rsp,%rbp
 * push ...
 *
 * I just grab pushes (aside from rbp)
 * */
static int test_instr_prologue(instr_t *instr)
{
	int opcode = instr_get_opcode(instr);

	if (opcode != OP_push) {
		return 0;
	}

	/* We don't want to mess with rbp */
	if (opnd_get_reg(instr_get_src(instr, 0)) == DR_REG_RBP) {
		return 0;
	}

	return 1;
}

/* Test for instructions in the epilogue */
UNUSED
static int test_instr_epilogue(UNUSED instr_t *instr)
{
	return 0;
}

static int test_instr_none(UNUSED instr_t *instr)
{
	return 0;
}

/* takes a callback to a function which tests an instruction for come condition
 * which determines if it stays in the set or not.
 *
 * Returns the number of bytes traversed while finding the next instruction set.
 * If an error occurs, the return value will be less than zero and its value
 * will be an error code.
 * */
static int next_set(byte **walk, byte *max, uintptr_t orig,
	struct instr_set *set, int (*test_instr)(instr_t *instr))
{
	instr_t *instr, *pinstr = NULL;
	int instr_len, nr_read = 0;

	set->start = set->end = orig;
	set->nr_instrs = 0;
	set->instrs = NULL;

	/* Alloc & init the instr */
	instr = instr_create(GLOBAL_DCONTEXT);
	if (!instr) {
		return -RAVE__ENOMEM;
	}

	while (*walk < max) {
		*walk = decode_from_copy(GLOBAL_DCONTEXT, *walk, PTR(orig), instr);
		instr_len = instr_length(GLOBAL_DCONTEXT, instr);

		if (NULL == *walk) {
			ERROR("Invalid instruction");
			nr_read = -RAVE__ETRANSFORM;
			goto err;
		}

		orig += instr_len;
		nr_read += instr_len;

		/* Test if we want to keep this instruction in the current set */
		if (NULL == test_instr || test_instr(instr)) {
			set->nr_instrs++;
			set->end = orig;

			/* dr instructions already have next/prev ptrs so we gonna use
			 * those. */
			if (NULL == set->instrs) {
				set->instrs = instr;
			} else {
				instr_set_prev(instr, pinstr);
				instr_set_next(pinstr, instr);
			}

			pinstr = instr;

			/* Allocate new instr */
			instr = instr_create(GLOBAL_DCONTEXT);
			if (NULL == instr) {
				nr_read = -RAVE__ENOMEM;
				goto err;
			}

			continue;
		}

		/* If we don't keep this instruction, break, unless this set is empty */
		if (set->nr_instrs) {
			break;
		}

		set->start = set->end = orig;
		instr_reuse(GLOBAL_DCONTEXT, instr);
	}

	instr_destroy(GLOBAL_DCONTEXT, instr);
	return nr_read;
err:
	if (instr) {
		instr_destroy(GLOBAL_DCONTEXT, instr);
	}

	for (instr_t *__instr = set->instrs;
		__instr;
		__instr = instr_get_next(__instr))
	{
		instr_destroy(GLOBAL_DCONTEXT, __instr);
	}
	return nr_read;
}

/* I remove the first two instructions of the prologue since I know they involve
 * saving the fbp (I just want pushes, but by finding the fbp preservation
 * instructions, I can more reliably find prologues). */
//TODO: static void prune_prologue(UNUSED struct instr_set *set);

/* This function populates the fields of the given transformable given a
 * function record and instruction bytes */
int transform_analyze(UNUSED transform_t self, const struct function *record,
	void *bytes, struct transformable *tf)
{
	byte *walk = bytes,
		 *end = OFFSET(walk, record->len);
	uintptr_t orig = record->addr;
	size_t length = record->len;
	struct instr_set current;
	int ret;

	/* First, we need to find the prologue (if there is one we can permute) */
	ret = next_set(&walk, end, orig, &tf->prologue,
		test_instr_prologue);
	if (ret < 0) {
		ERROR("error while finding function prologue");
		return -ret;
	}

	length -= ret;

	/* If there was no prologue, then we can't transform this function */
	if (0 == tf->prologue.nr_instrs) {
		DEBUG("Function has no randomizable prologue");
		return RAVE__ETRANSFORM;
	}

	/* Now, we find any other instruction sets that mirror the prologue (i.e.
	 * find any epilogues in the function) */
	while (walk < end) {
		length -= next_set(&walk, end, orig, &current, test_instr_none);
	}

	/* There should be no unnacounted for bytes in this function */
	if (length) {
		ERROR("Function size not true");
		return RAVE__ETRANSFORM;
	}

	/* All is well, populate the transformable function */
	memcpy(&tf->record, record, sizeof(*record));

	DEBUG_BLOCK(
		instr_t *__instr;

		DEBUG("");
		fprintf(stderr, "\tAnalysis of function @ 0x%"PRIxPTR", size = %zu\n",
			record->addr, record->len);
		fprintf(stderr, "\tHas prologue 0x%"PRIxPTR" - 0x%"PRIxPTR" (%zu instructions)\n",
			tf->prologue.start, tf->prologue.end, tf->prologue.nr_instrs);

		for (__instr = tf->prologue.instrs;
			__instr;
			__instr = instr_get_next(__instr))
		{
			fprintf(stderr, "\t\t");
			instr_disassemble(GLOBAL_DCONTEXT, __instr, STDERR);
			fprintf(stderr, "\n");
		}
	)

	return RAVE__SUCCESS;
}

int transform_permute(UNUSED struct transform *self, struct transformable *tf)
{
	(void)tf;
	return RAVE__SUCCESS;
}
