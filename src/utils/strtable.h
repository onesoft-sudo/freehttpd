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

#ifndef FHTTPD_STRTABLE_H
#define FHTTPD_STRTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STRTABLE_DEFAULT_CAPACITY 16

struct strtable_entry
{
	char *key;
	void *data;
	struct strtable_entry *next;
	struct strtable_entry *prev;
};

struct strtable
{
	size_t capacity;
	size_t count;
	struct strtable_entry *head;
	struct strtable_entry *tail;
	struct strtable_entry *buckets;
};

struct strtable *strtable_create (size_t capacity);
void strtable_destroy (struct strtable *table);
void *strtable_get (struct strtable *table, const char *key);
bool strtable_set (struct strtable *table, const char *key, void *data);
void *strtable_remove (struct strtable *table, const char *key);
bool strtable_resize (struct strtable *table, size_t new_capacity);
bool strtable_contains (struct strtable *table, const char *key);

#endif /* FHTTPD_STRTABLE_H */
