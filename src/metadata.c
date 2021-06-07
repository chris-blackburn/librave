#include <libdwarf/dwarf.h>
#include <libdwarf/libdwarf.h>

#include "binary.h"
#include "rave/errno.h"
#include "log.h"

struct metadata {
	struct binary *binary;

	Dwarf_Debug dbg;
};

int metadata_list_functions(struct metadata *self)
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

	for (size_t i = 0;; i++) {
		Dwarf_Die no_die = 0;
		Dwarf_Die cu_die = 0;
		Dwarf_Die child_die = 0;

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
			break;
		}

		/* The header cu will have one sibling which is the cu die */
		rc = dwarf_siblingof_b(dbg, no_die, is_info, &cu_die, &err);
		if (rc == DW_DLV_ERROR) {
			goto dwarf_err;
		} else if (rc == DW_DLV_NO_ENTRY) {
			ERROR("dwarf: no sibling die for top level cu");
			return RAVE__EDWARF;
		}

		/* iterate through all children to find subprograms */
		for (rc = dwarf_child(cu_die, &child_die, &err);
			rc != DW_DLV_NO_ENTRY;
			rc = dwarf_siblingof_b(dbg, child_die, is_info, &child_die, &err))
		{
			Dwarf_Half tag;

			if (rc == DW_DLV_ERROR) {
				goto dwarf_err;
			}

			/* If we have a function, get the function info */
			rc = dwarf_tag(child_die, &tag, &err);
			if (dwarf_tag(child_die, &tag, &err) != DW_DLV_OK) {
				goto dwarf_err;
			}

			/* Only interested in subprogram DIEs here */
			if (tag != DW_TAG_subprogram) {
				continue;
			}

			// TODO: move to other function
			{
				Dwarf_Addr lo, hi;
				Dwarf_Half form = 0;
				enum Dwarf_Form_Class formclass = 0;
				rc = dwarf_lowpc(child_die, &lo, &err);
				rc = dwarf_highpc_b(child_die, &hi, &form, &formclass, &err);

				/* high pc may actually just be the length of the function */
				if (formclass == DW_FORM_CLASS_CONSTANT) {
					hi += lo;
				}

				DEBUG("func: %llx <-> %llx", lo, hi);
			}
		}
	}

	return RAVE__SUCCESS;
dwarf_err:
	ERROR("dwarf: %s", dwarf_errmsg(err));
	dwarf_dealloc(dbg, err, DW_DLA_ERROR);
	return RAVE__EDWARF;
}

int metadata_init(struct metadata *self, struct binary *binary)
{
	Dwarf_Debug dbg = 0;
	Dwarf_Error err = 0;
	// TODO: remove
	struct metadata m;
	self = &m;

	if (NULL == self) {
		//return RAVE__EINVAL;
	}

	if (dwarf_elf_init(binary->elf, DW_DLC_READ, 0, 0, &dbg, &err) !=
		DW_DLV_OK)
	{
		ERROR("Failed to init dwarf");
		dwarf_dealloc(dbg, err, DW_DLA_ERROR);
		return RAVE__EDWARF;
	}

	DEBUG("Metadata initialized");
	self->dbg = dbg;
	self->binary = binary;

	// TODO: remove
	return metadata_list_functions(self);

	return RAVE__SUCCESS;
}

int metadata_close(struct metadata *self)
{
	Dwarf_Debug dbg = self->dbg;
	Dwarf_Error err = NULL;

	if (NULL == self) {
		return RAVE__EINVAL;
	}

    if (dwarf_finish(dbg, &err) != DW_DLV_OK) {
		ERROR("Failed to close dwarf");
		dwarf_dealloc(dbg, err, DW_DLA_ERROR);
        return RAVE__EDWARF;
    }

	return RAVE__SUCCESS;
}
