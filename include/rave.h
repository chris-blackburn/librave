/**
 * librave
 *
 * public api
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/1/1977
 */

#ifndef __LIBRAVE_H_
#define __LIBRAVE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Opaque handle */
typedef struct rave_handle rave_handle_t;

int rave_init(rave_handle_t *self, const char *filename);
int rave_close(rave_handle_t *self);

void *rave_handle_fault(rave_handle_t *self, uintptr_t address);
void *rave_get_code(rave_handle_t *self, size_t *length);

/* User allocates the opaque type */
// TODO: can we static allocate this? I can't remember how..
size_t rave_handle_size(void);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRAVE_H_ */
