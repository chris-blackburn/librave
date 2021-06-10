/**
 * Function
 *
 * Representation of a function
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/1/1977
 */

#ifndef __FUNCTION_H_
#define __FUNCTION_H_

#include <stdint.h>

struct function {
	uintptr_t addr;
	size_t len;
};

#endif /* __FUNCTION_H_ */
