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

#ifndef FHTTPD_BITMAP_H
#define FHTTPD_BITMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct bitmap
{
	uint64_t *bits;
	size_t size;
	size_t bit_size : 6;
};

typedef struct bitmap bitmap_t;

bitmap_t *bitmap_create (bitmap_t *bitmap);
void bitmap_init (bitmap_t *bitmap);
void bitmap_free (bitmap_t *bitmap, bool in_heap);
bool bitmap_set (bitmap_t *bitmap, size_t pos, bool bit);
bool bitmap_get (bitmap_t *bitmap, size_t pos);
void bitmap_print (bitmap_t *bitmap);

#endif /* FHTTPD_BITMAP_H */
