#include <stdlib.h>

#include "random.h"

static inline void swap(int *a, int *b)
{
	int tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
}

void shuffle(int *arr, size_t nmemb)
{
	int rnd;

	for (size_t i = 0; i < nmemb - 1; i++) {
		rnd = i + (rand() % (nmemb - i));
		swap(&arr[i], &arr[rnd]);
	}
}
