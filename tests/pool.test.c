#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "pool.h"

int
main (void)
{
	struct fh_pool *root_pool = fh_pool_create (0);
	assert (root_pool != NULL);

	uint32_t *ints[5000];

	/* Allocation tests */

	for (size_t i = 0; i < 5000; i++)
	{
		ints[i] = fh_pool_alloc (root_pool, sizeof (uint32_t));
		assert (ints[i] != NULL);
		*ints[i] = i * 2;
	}

	for (size_t i = 0; i < 5000; i++)
	{
		assert (*ints[i] == i * 2);
	}

	fh_pool_destroy (root_pool);
	return 0;
}
