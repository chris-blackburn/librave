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

typedef struct transform * transform_t;

int transform_init(transform_t self);

/* There are a few things we want to verify in this function:
 * 1. Ensure the stack delta is 0
 *
 * If one of these is not true, then we can not safely transform the function.
 **/
int transform_is_safe(transform_t self, uintptr_t orig,
	void *function, size_t length);

#endif /* __TRANSFORM_H_ */

