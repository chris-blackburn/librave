#include <inttypes.h>

/* transfomer for x86 64 powered by DynamoRIO standalone disassembler */
#define LINUX
#define X86_64
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

/* returns the stack delta resulting from a single instruction */
static int get_stack_delta(instr_t *instr)
{
	int opcode;

	/* If we don't write to the stack pointer nothing to worry about here */
	if (!instr_writes_to_reg(instr, DR_REG_XSP, DR_QUERY_INCLUDE_ALL)) {
		return 0;
	}

	/* We don't need to track call's disturbance of the stack pointer */
	if (instr_is_call(instr)) {
		return 0;
	}

	/* Returns always pop the return address off the stack */
	if (instr_is_return(instr)) {
		return 8;
	}

	/* For any other instructions, we have to be more precarious */
	opcode = instr_get_opcode(instr);
	switch (opcode) {
	/* push and pop size determined through operand size */
	case OP_push:
		return -opnd_size_in_bytes(opnd_get_size(instr_get_src(instr, 0)));
	case OP_pop:
		return opnd_size_in_bytes(opnd_get_size(instr_get_dst(instr, 0)));
	case OP_pushf:
		return -8;
	case OP_popf:
	case OP_leave:
		return 8;
	/* For add and sub, we want to get the immediate value */
	case OP_add:
	case OP_sub:
		// TODO: This is safe to ignore for now since I'm not concerned with the
		// whole stack frame, just want to make sure I have a valid function.
		// However, if you wanted to mess with stack slots more aggressively
		// (would likely require compiler support), then you would want to track
		// this.
		return 0;
	default:
		WARN("Stack delta modified by uncaught instruction dr op = %d", opcode);
		return 0;
	}
}

int transform_is_safe(UNUSED struct transform *self, uintptr_t orig,
	void *function, size_t length)
{
	instr_noalloc_t noalloc;
	instr_t *instr;
	byte *walk = function,
		 *end = OFFSET(function, length);
	int stack_delta,
		instr_len;

	instr_noalloc_init(GLOBAL_DCONTEXT, &noalloc);
	instr = instr_from_noalloc(&noalloc);

	/* For x86, the address of the caller will be at the top of our frame */
	stack_delta = -8;

	/* Loop through each instruction and analyze */
	while (walk < end) {
		walk = decode_from_copy(GLOBAL_DCONTEXT, walk, PTR(orig), instr);
		instr_len = instr_length(GLOBAL_DCONTEXT, instr);

		if (NULL == walk) {
			ERROR("Invalid instruction");
			return RAVE__ETRANSFORM;
		}

		orig += instr_len;
		length -= instr_len;

		/* analyze */
		stack_delta += get_stack_delta(instr);

		instr_reset(GLOBAL_DCONTEXT, instr);
	}

	/* There should be no unnacounted for bytes in this function */
	if (length) {
		ERROR("Function size not true");
		return RAVE__ETRANSFORM;
	}

	/* If the stack delta is not zero, then we cannot safely manipulate this
	 * function */
	if (stack_delta) {
		ERROR("Cannot resolve stack delta");
		return RAVE__ETRANSFORM;
	}

	return RAVE__SUCCESS;
}
