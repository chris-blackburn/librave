/**
 * Util
 *
 * Various utilities
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/1/1977
 */

#ifndef __UTIL_H_
#define __UTIL_H_

#define PAGESZ 4096
#define PAGE_DOWN(addr) ((addr) & ~(PAGESZ - 1))
#define PAGE_UP(addr) PAGE_DOWN((addr) + (PAGESZ - 1))

/* Offset a pointer by certain number of bytes */
#define OFFSET(ptr, off) (__typeof__ (ptr))((uintptr_t)(ptr) + (off))
#define PTR(val) (void *)(val)
#define CONTAINS(val, lo, hi) ((uintptr_t)(lo) <= (uintptr_t)(val) && \
	(uintptr_t)(val) < (uintptr_t)(hi))

#define min(a, b) \
	({__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a < _b ? _a : _b; })

#define max(a, b) \
	({__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a > _b ? _a : _b; })

#define container_of(ptr, type, member) ({ \
	void *__mptr = (void *)(ptr); \
	((type *)(__mptr - offsetof(type, member))); })

#endif /* __UTIL_H_ */

