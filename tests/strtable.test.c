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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hash/strtable.h"

int
main (void)
{
	srand ((unsigned int) time (NULL));

	struct strtable *table = strtable_create (0);

	assert (table != NULL);

	strtable_set (table, "Name", (void *) "test");
	assert (strtable_get (table, "Name") == (void *) "test");

	const size_t count = 10000;

	char *keys[count];
	char data[count];

	memset (data, 0, count);

	FILE *f = fopen ("/dev/urandom", "r");

	assert (f != NULL);

	for (size_t i = 0; i < count; i++)
	{
		size_t len = (size_t) (rand () % (128 - 32 + 1) + 32);
		keys[i] = malloc (len + 1);
		assert (keys[i] != NULL);

		for (size_t j = 0; j < len;)
		{
			char c;
			fread (&c, 1, 1, f);

			if (c >= 32 && c <= 126)
			{
				keys[i][j++] = c;
			}
		}

		keys[i][len] = 0;
		printf ("[%zu] Setting key: \"%s\" = %p\n", i, keys[i], (void *) &data[i]);
		assert (strtable_set (table, keys[i], &data[i]) == true);
	}

	fclose (f);

	for (size_t i = 0; i < count; i++)
	{
		printf ("[%zu] Getting key: \"%s\" [== %p]\n", i, keys[i], (void *) &data[i]);
		void *p = strtable_get (table, keys[i]);
		printf ("[%zu] Result: %p\n", i, p);
		bool contains = strtable_contains (table, keys[i]);
		assert (p == &data[i]);
		assert (contains == true);
	}

	for (size_t i = 0; i < count; i++)
	{
		if (i % 2 == 0)
		{
			printf ("[%zu] Removing key \"%s\"\n", i, keys[i]);
			assert (strtable_remove (table, keys[i]) == &data[i]);
		}
	}

	for (size_t i = 0; i < count; i++)
	{
		if (i % 2 == 0)
		{
			printf ("[%zu] Checking key \"%s\" [even]\n", i, keys[i]);
			assert (strtable_get (table, keys[i]) == NULL);
			assert (strtable_contains (table, keys[i]) == false);
		}
		else
		{
			printf ("[%zu] Checking key \"%s\" [odd]\n", i, keys[i]);
			assert (strtable_get (table, keys[i]) == &data[i]);
			assert (strtable_contains (table, keys[i]) == true);
		}
	}

	for (size_t i = 0; i < count; i++)
	{
		free (keys[i]);
	}

	strtable_destroy (table);
	return 0;
}
