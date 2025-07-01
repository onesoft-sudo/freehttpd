/*
 * This file is part of OSN freehttpd.
 * 
 * Copyright (C) 2025  OSN Developers.
 *
 * OSN freehttpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * OSN freehttpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.
 */

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
