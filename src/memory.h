/**
 * Memory
 *
 * Memory management. The intent is to allow the user to load in their own
 * memory management instead of using stdlib, so we abstract.
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/1/1977
 */

#ifndef __MEMORY_H_
#define __MEMORY_H_

#include <stdlib.h>

// TODO: don't just use macros - allow user to swap in their own mm functions
#define rave_malloc(x) malloc(x)
#define rave_free(x) ({if (x) free(x);})

#endif /* __MEMORY_H_ */

