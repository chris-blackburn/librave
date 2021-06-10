#include <libdwarf/dwarf.h>
#include <libdwarf/libdwarf.h>

#include "metadata.h"
#include "memory.h"
#include "rave/errno.h"
#include "log.h"


struct metadata {
	struct binary *binary;

	Dwarf_Debug dbg;
};

static int get_function_hilo(struct metadata *self, Dwarf_Die die,
	foreach_function_cb cb, void *arg)
{
	Dwarf_Debug dbg = self->dbg;
	Dwarf_Addr lo, hi;
	Dwarf_Half form = 0;
	enum Dwarf_Form_Class formclass = 0;
	Dwarf_Error err;
	struct function function;
	int rc;

	/* If the subprogram does not have an address, then it is probably inlined
	 * or is special in some other way. We want to skip those. */
	rc = dwarf_lowpc(die, &lo, &err);
	if (rc == DW_DLV_ERROR) {
		goto dwarf_err;
	} else if (rc == DW_DLV_NO_ENTRY) {
		return RAVE__SUCCESS;
	}

	rc = dwarf_highpc_b(die, &hi, &form, &formclass, &err);
	if (rc == DW_DLV_ERROR) {
		goto dwarf_err;
	} else if (rc == DW_DLV_NO_ENTRY) {
		return RAVE__SUCCESS;
	}

	/* high pc may actually just be the length of the function */
	if (formclass == DW_FORM_CLASS_CONSTANT) {
		hi += lo;
	}

	function.addr = lo;
	function.len = hi - lo;
	return cb(&function, arg);
dwarf_err:
	ERROR("dwarf: %s", dwarf_errmsg(err));
	dwarf_dealloc(dbg, err, DW_DLA_ERROR);
	return RAVE__EDWARF;
}

/* Not as useful as I originally thought it would be - only works per
 * compilation unit */
#if 0
static int get_subprogram_count(Dwarf_Debug dbg, Dwarf_Unsigned abbrev_offset,
	size_t *nfuncs)
{
	Dwarf_Abbrev abbrev = 0;
	Dwarf_Unsigned length = 0;
	Dwarf_Unsigned attr_count = 0;
	Dwarf_Half tag = 0;
	Dwarf_Error err;
	int rc;

	*nfuncs = 0;

	while (1) {
		rc = dwarf_get_abbrev(dbg,
			abbrev_offset,
			&abbrev,
			&length,
			&attr_count,
			&err);
		if (rc == DW_DLV_ERROR) {
			goto dwarf_err;
		} else if (rc == DW_DLV_NO_ENTRY) {
			/* No entries left */
			break;
		}

		rc = dwarf_get_abbrev_tag(abbrev, &tag, &err);
		if (rc != DW_DLV_OK) {
			goto dwarf_err;
		}

		/* Only interested in subprogram DIEs here */
		if (tag == DW_TAG_subprogram) {
			(*nfuncs)++;
		}

		dwarf_dealloc(dbg, abbrev, DW_DLA_ABBREV);
		abbrev_offset += length;
	}

	DEBUG("Found %lu functions", *nfuncs);

	return RAVE__SUCCESS;
dwarf_err:
	ERROR("dwarf: %s", dwarf_errmsg(err));
	dwarf_dealloc(dbg, err, DW_DLA_ERROR);
	return RAVE__EDWARF;
}
#endif

static int process_die_and_siblings(struct metadata *self, Dwarf_Die cu_die,
	Dwarf_Bool is_info, foreach_function_cb cb, void *arg)
{
	Dwarf_Debug dbg = self->dbg;
	Dwarf_Die next_die = 0;
	Dwarf_Die cur_die = 0;
	Dwarf_Half tag;
	Dwarf_Error err;
	int rc;

	rc = dwarf_child(cu_die, &cur_die, &err);

	/* iterate through all children to find subprograms */
	while (rc != DW_DLV_NO_ENTRY) {
		if (rc == DW_DLV_ERROR) {
			goto dwarf_err;
		}

		/* Only interested in subprogram DIEs here */
		rc = dwarf_tag(cur_die, &tag, &err);
		if (rc != DW_DLV_OK) {
			dwarf_dealloc(dbg, cur_die, DW_DLA_DIE);
			goto dwarf_err;
		}

		/* Get relevant info */
		if (tag == DW_TAG_subprogram) {
			rc = get_function_hilo(self, cur_die, cb, arg);
			if (rc != RAVE__SUCCESS) {
				dwarf_dealloc(dbg, cur_die, DW_DLA_DIE);
				return rc;
			}
		}

		rc = dwarf_siblingof_b(dbg, cur_die, is_info, &next_die, &err);
		dwarf_dealloc(dbg, cur_die, DW_DLA_DIE);
		cur_die = next_die;
	}

	return RAVE__SUCCESS;
dwarf_err:
	ERROR("dwarf: %s", dwarf_errmsg(err));
	dwarf_dealloc(dbg, err, DW_DLA_ERROR);
	return RAVE__EDWARF;
}

static int foreach_function(struct metadata *self, foreach_function_cb cb,
	void *arg)
{
	Dwarf_Debug dbg = self->dbg;
	Dwarf_Unsigned cu_header_length = 0;
	Dwarf_Unsigned abbrev_offset = 0;
	Dwarf_Half address_size = 0;
	Dwarf_Half version_stamp = 0;
	Dwarf_Half length_size = 0;
	Dwarf_Half extension_size = 0;
	Dwarf_Sig8 signature = {0};
	Dwarf_Unsigned typeoffset = 0;
	Dwarf_Unsigned next_cu_header_offset = 0;
	Dwarf_Half header_cu_type = DW_UT_compile;
	Dwarf_Bool is_info = 1;
	Dwarf_Error err;
	int rc;

	DEBUG("dwarf searching for functions");
	while (1) {
		Dwarf_Die cu_die = 0;

		rc = dwarf_next_cu_header_d(self->dbg,
				is_info,
				&cu_header_length,
				&version_stamp,
				&abbrev_offset,
				&address_size,
				&length_size,
				&extension_size,
				&signature,
				&typeoffset,
				&next_cu_header_offset,
				&header_cu_type,
				&err);
		if (rc == DW_DLV_ERROR) {
			goto dwarf_err;
		} else if (rc == DW_DLV_NO_ENTRY) {
			/* No entries left */
			dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
			break;
		}

		/* The header cu will have one sibling which is the cu die */
		rc = dwarf_siblingof_b(dbg, 0, is_info, &cu_die, &err);
		if (rc == DW_DLV_ERROR) {
			goto dwarf_err;
		} else if (rc == DW_DLV_NO_ENTRY) {
			WARN("dwarf: missing cu die...");
			continue;
		}

		/* Process this cu to find any functions and grab relevant metadata */
		rc = process_die_and_siblings(self, cu_die, is_info, cb, arg);
		dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
		if (rc != RAVE__SUCCESS) {
			return rc;
		}
	}

	return RAVE__SUCCESS;
dwarf_err:
	ERROR("dwarf: %s", dwarf_errmsg(err));
	dwarf_dealloc(dbg, err, DW_DLA_ERROR);
	return RAVE__EDWARF;
}

static int init(struct metadata *self, struct binary *binary)
{
	Dwarf_Debug dbg = 0;
	Dwarf_Error err = 0;

	if (NULL == self) {
		return RAVE__EINVAL;
	}

	if (dwarf_elf_init(binary->elf, DW_DLC_READ, 0, 0, &dbg, &err) !=
		DW_DLV_OK)
	{
		ERROR("Failed to init dwarf");
		dwarf_dealloc(dbg, err, DW_DLA_ERROR);
		return RAVE__EDWARF;
	}

	DEBUG("Dwarf metadata initialized");
	self->dbg = dbg;
	self->binary = binary;

	return RAVE__SUCCESS;
}

static int close(struct metadata *self)
{
	Dwarf_Error err = NULL;

	if (NULL == self) {
		return RAVE__EINVAL;
	}

	if (dwarf_finish(self->dbg, &err) != DW_DLV_OK) {
		ERROR("Failed to close dwarf");
		dwarf_dealloc(self->dbg, err, DW_DLA_ERROR);
		return RAVE__EDWARF;
	}

	return RAVE__SUCCESS;
}

static struct metadata *create()
{
	return rave_malloc(sizeof(struct metadata));
}

static void destroy(struct metadata *self)
{
	if (NULL != self) {
		rave_free(self);
	}
}

struct metadata_op metadata_dwarf = {
	.create = create,
	.destroy = destroy,
	.init = init,
	.close = close,
	.foreach_function = foreach_function,
};
