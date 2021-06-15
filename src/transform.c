#include <string.h>
#include <inttypes.h>

#define X86_64
#define LINUX
#include <dr_api.h>

#include "transform.h"
#include "rave/errno.h"
#include "memory.h"
#include "util.h"
#include "log.h"

#define instr_for_each(cursor, set) \
	for (cursor = (set)->instrs; cursor; cursor = instr_get_next(cursor))

/* Main transform handler */
struct transform {
	// TODO: turn into a hashlist
	struct list_head transformables;
};

static struct instr_set * instr_set_create(void)
{
	return rave_malloc(sizeof(struct instr_set));
}

static void instr_set_destroy(struct instr_set *self)
{
	if (NULL != self) {
		rave_free(self);
	}
}

static struct transformable * transformable_create(void)
{
	return rave_malloc(sizeof(struct transformable));
}

static void transformable_destroy(struct transformable *self)
{
	if (NULL != self) {
		rave_free(self);
	}
}

static void instr_set_init(struct instr_set *self, uintptr_t orig)
{
	if (NULL == self) {
		return;
	}

	self->start = self->end = orig;
	self->nr_instrs = 0;
	self->instrs = NULL;
}

static void instr_set_close(struct instr_set *self)
{
	instr_t * instr;

	if (NULL == self) {
		return;
	}

	for (instr = self->instrs;
		instr;
		instr = instr_get_next(instr))
	{
		instr_destroy(GLOBAL_DCONTEXT, instr);
	}

	self->instrs = NULL;
}

static void transformable_init(struct transformable *self,
	const struct function *record)
{
	if (NULL == self) {
		return;
	}

	memcpy(&self->record, record, sizeof(struct function));
	instr_set_init(&self->prologue, record->addr);
	INIT_LIST_HEAD(&self->epilogues);
}

static void transformable_close(struct transformable *self)
{
	struct list_head *pos, *n;
	struct instr_set *set;

	if (NULL == self) {
		return;
	}

	instr_set_close(&self->prologue);

	list_for_each_safe(pos, n, &self->epilogues) {
		list_del(pos);
		set = list_entry(pos, struct instr_set, l);
		instr_set_close(set);
		instr_set_destroy(set);
	}
}

struct transform * transform_create(void)
{
	return rave_malloc(sizeof(struct transform));
}

void transform_destroy(struct transform *self)
{
	if (NULL != self) {
		rave_free(self);
	}
}

int transform_init(struct transform *self)
{
	if (NULL == self) {
		return RAVE__EINVAL;
	}

	if (!dr_set_isa_mode(GLOBAL_DCONTEXT, DR_ISA_AMD64, NULL)) {
		return RAVE__ETRANSFORM;
	}

	INIT_LIST_HEAD(&self->transformables);

	return RAVE__SUCCESS;
}

int transform_close(struct transform *self)
{
	struct list_head *pos, *n;
	struct transformable *tf;

	if (NULL == self) {
		return RAVE__EINVAL;
	}

	list_for_each_safe(pos, n, &self->transformables) {
		list_del(pos);
		tf = list_entry(pos, struct transformable, l);
		transformable_close(tf);
	}

	return RAVE__SUCCESS;
}

/* Test for instructions could be in the prologue. Should look like:
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

/* Test for instructions that could be in the epilogue */
static int test_instr_epilogue(instr_t *instr)
{
	int opcode = instr_get_opcode(instr);

	if (opcode != OP_pop) {
		return 0;
	}

	/* We don't want to mess with rbp */
	if (opnd_get_reg(instr_get_dst(instr, 0)) == DR_REG_RBP) {
		return 0;
	}

	return 1;
}

/* takes a callback to a function which tests an instruction for come condition
 * which determines if it stays in the set or not. */
static int next_set(byte **walk, byte *max, uintptr_t orig,
	struct instr_set *set, int (*test_instr)(instr_t *instr))
{
	instr_t *instr = NULL, *pinstr = NULL;
	int instr_len;
	int ret;

	/* Alloc & init the instr */
	instr = instr_create(GLOBAL_DCONTEXT);
	if (!instr) {
		return RAVE__ENOMEM;
	}

	instr_set_init(set, orig);

	while (*walk < max) {
		*walk = decode_from_copy(GLOBAL_DCONTEXT, *walk, PTR(orig), instr);
		instr_len = instr_length(GLOBAL_DCONTEXT, instr);

		if (NULL == *walk) {
			ERROR("Invalid instruction");
			ret = RAVE__ETRANSFORM;
			goto err;
		}

		orig += instr_len;

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

			/* Allocate raw bits separately (so we don't mangle the backing
			 * store of instruction data) */
			instr_allocate_raw_bits(GLOBAL_DCONTEXT, instr, instr_len);
			pinstr = instr;

			/* Allocate new instr */
			instr = instr_create(GLOBAL_DCONTEXT);
			if (NULL == instr) {
				ret = RAVE__ENOMEM;
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
	return RAVE__SUCCESS;
err:
	if (instr) {
		instr_destroy(GLOBAL_DCONTEXT, instr);
	}

	instr_set_close(set);
	return ret;
}

static int is_epilogue(const struct instr_set *pro, const struct instr_set *epi)
{
	instr_t *iter;
	reg_id_t regs[pro->nr_instrs], *reg = regs; // TODO: bleh

	if (pro->nr_instrs != epi->nr_instrs) {
		return 0;
	}

	/* Check the order */
	for (iter = pro->instrs;
		iter;
		iter = instr_get_next(iter), reg++)
	{
		*reg = opnd_get_reg(instr_get_src(iter, 0));
	}

	reg--;

	for (iter = epi->instrs;
		iter;
		iter = instr_get_next(iter), reg--)
	{
		if (*reg != opnd_get_reg(instr_get_dst(iter, 0))) {
			DEBUG("Epilogue doesn't match prologue order");
			return 0;
		}
	}

	return 1;
}

/* This function populates the fields of the given transformable given a
 * function record and instruction bytes */
int transform_add_function(transform_t self, const struct function *record,
	void *bytes)
{
	byte *walk = bytes,
		 *end = OFFSET(walk, record->len);
	uintptr_t orig = record->addr;
	struct transformable *tf;
	struct instr_set *set = NULL;
	int rc, ret;

	tf = transformable_create();
	if (NULL == tf) {
		return RAVE__ENOMEM;
	}
	transformable_init(tf, record);

	/* First, we need to find the prologue (if there is one we can permute) */
	rc = next_set(&walk, end, orig, &tf->prologue, test_instr_prologue);
	if (rc != RAVE__SUCCESS) {
		ERROR("error while finding function prologue");
		return rc;
	}

	/* If there was no prologue (or if it was too small), then we can't
	 * transform this function */
	if (tf->prologue.nr_instrs < 2) {
		DEBUG("Function has no randomizable prologue");
		ret = RAVE__ETRANSFORM;
		goto err;
	}

	/* Allocate instruction set to iterate over remaining sets */
	set = instr_set_create();
	if (NULL == set) {
		ret = RAVE__ENOMEM;
		goto err;
	}

	/* Now, we find any other instruction sets that mirror the prologue (i.e.
	 * find any epilogues in the function) */
	while (walk < end) {
		rc = next_set(&walk, end, orig, set, test_instr_epilogue);
		if (rc != RAVE__SUCCESS) {
			ERROR("error while finding function next instruction set");
			ret = rc;
			goto err;
		}

		/* Now, we need to check if this candidate is truly an epilogue */
		if (is_epilogue(&tf->prologue, set)) {
			/* Spin off */
			DEBUG("Found matching epilogue");
			list_add_tail(&set->l, &tf->epilogues);

			set = instr_set_create();
			if (NULL == set) {
				ret = RAVE__ENOMEM;
				goto err;
			}

			continue;
		}

		instr_set_close(set);
	}

	instr_set_destroy(set);
	set = NULL;

	/* There should be no unnacounted for bytes in this function */
	if (walk != end) {
		ERROR("Function size not true");
		ret = RAVE__ETRANSFORM;
		goto err;
	}

	if (list_empty(&tf->epilogues)) {
		ERROR("Found no matching epilogues");
		ret = RAVE__ETRANSFORM;
		goto err;
	}

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

	list_add_tail(&tf->l, &self->transformables);
	return RAVE__SUCCESS;
err:
	instr_set_close(set);
	instr_set_destroy(set);
	transformable_close(tf);
	transformable_destroy(tf);
	return ret;
}

static int instr_set_encode(const struct instr_set *set, byte *target)
{
	instr_t *instr;
	uintptr_t orig;
	byte *walk = target, *prev;

	orig = set->start;
	instr_for_each(instr, set) {
		prev = walk;
		walk = instr_encode_to_copy(GLOBAL_DCONTEXT, instr, walk, PTR(orig));
		if (NULL == walk) {
			ERROR("Could not encode instr");
			return RAVE__ETRANSFORM;
		}

		orig += walk - prev;
		if (orig > set->end) {
			WARN("Expected fewer instructions during encode");
			return RAVE__ETRANSFORM;
		}
	}

	return RAVE__SUCCESS;
}

/* Don't do any transformation, just re-encode */
static int identity(struct transformable *tf, struct window *fw)
{
	struct instr_set *set;
	int rc;

	/* Identity encode the prologue */
	set = &tf->prologue;
	rc = instr_set_encode(set, window_view(fw, set->start, NULL));
	if (rc != RAVE__SUCCESS) {
		ERROR("Could not encode prologue @ 0x%"PRIxPTR" size = %d",
			set->start, (int)(set->end - set->start));
		return rc;
	}

	/* Encode all the epilogues */
	list_for_each_entry(set, &tf->epilogues, l) {
		rc = instr_set_encode(set, window_view(fw, set->start, NULL));
		if (rc != RAVE__SUCCESS) {
			ERROR("Could not encode instruction set @ 0x%"PRIxPTR" size = %d",
				set->start, (int)(set->end - set->start));
			return rc;
		}
	}

	return RAVE__SUCCESS;
}

/* Permute all prologues and epilogues. new instructions encoded to the target
 * text */
int transform_permute_all(struct transform *self, struct window *text)
{
	struct transformable *tf;
	struct window fw;
	void *bytes;
	int rc;

	if (NULL == self || NULL == text) {
		return RAVE__EINVAL;
	}

	DEBUG("Permuting all function preservation code");

	list_for_each_entry(tf, &self->transformables, l) {
		bytes = window_view(text, tf->record.addr, NULL);
		window_init(&fw, tf->record.addr, bytes, tf->record.len);

		rc = identity(tf, &fw);
		if (rc != RAVE__SUCCESS) {
			return rc;
		}
	}

	DEBUG("done!");

	return RAVE__SUCCESS;
}
