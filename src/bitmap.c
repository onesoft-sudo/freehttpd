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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitmap.h"

bitmap_t *
bitmap_create (bitmap_t *bitmap)
{
	if (!bitmap)
		bitmap = malloc (sizeof (*bitmap));

	if (!bitmap)
		return NULL;

	bitmap->size = 0;
	bitmap->bit_size = 0;
	bitmap->bits = NULL;

	return bitmap;
}

void
bitmap_free (bitmap_t *bitmap, bool in_heap)
{
	free (bitmap->bits);

	if (in_heap)
		free (bitmap);
}

bool
bitmap_get (bitmap_t *bitmap, size_t pos)
{
	size_t i = pos >> 6;

	if (i >= bitmap->size)
		return false;

	uint64_t bits = bitmap->bits[i];
	uint64_t offset = pos & 0x3F;

	return bits & (0x1ULL << (0x3F - offset));
}

bool
bitmap_set (bitmap_t *bitmap, size_t pos, bool bit)
{
	size_t i = pos >> 6;

	if (i >= bitmap->size)
	{
		uint64_t *bits = realloc (bitmap->bits, sizeof (uint64_t) * (i + 1));

		if (!bits)
			return false;

		memset (bits + bitmap->size, 0, sizeof (uint64_t) * (i + 1 - bitmap->size));
		bitmap->bits = bits;
		bitmap->size = i + 1;
		bitmap->bit_size = 0;
	}

	bit &= 0x1;

	uint64_t bits = bitmap->bits[i];
	uint64_t offset = pos & 0x3F;
	bool old_value = bits & (0x1ULL << (0x3F - offset));

	if (offset >= bitmap->bit_size)
		bitmap->bit_size = offset + 1;

	if (bit)
		bits |= (0x1ULL) << (0x3F - offset);
	else
		bits &= ~((0x1ULL) << (0x3F - offset));

	bitmap->bits[i] = bits;
	return old_value;
}

void
bitmap_print (bitmap_t *bitmap)
{
	printf ("<bitmap [%zu bits] ",
			bitmap->size == 0 ? 0 : (((bitmap->size - 1) * 64) + (bitmap->bit_size == 0 ? 64 : bitmap->bit_size)));

	for (size_t i = 0; i < bitmap->size; i++)
	{
		uint64_t bits = bitmap->bits[i];
		size_t bit_size = i == (bitmap->size - 1) ? bitmap->bit_size == 0 ? 0x40 : bitmap->bit_size : 0x40;

		for (size_t j = 0; j < bit_size; j++)
			fputc (bits & (1ULL << (0x3F - j)) ? '1' : '0', stdout);
	}

	printf (">\n");
}
