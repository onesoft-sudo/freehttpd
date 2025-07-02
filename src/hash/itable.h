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

#ifndef FHTTPD_ITABLE_H
#define FHTTPD_ITABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ITABLE_DEFAULT_CAPACITY 16

struct itable_entry
{
	uint64_t key;
	void *data;
	struct itable_entry *next;
	struct itable_entry *prev;
};

struct itable
{
	uint64_t capacity;
	uint64_t count;
	struct itable_entry *head;
	struct itable_entry *tail;
	struct itable_entry *buckets;
};

struct itable *itable_create (uint64_t capacity);
void itable_destroy (struct itable *table);
void *itable_get (struct itable *table, uint64_t key);
bool itable_set (struct itable *table, uint64_t key, void *data);
void *itable_remove (struct itable *table, uint64_t key);
bool itable_resize (struct itable *table, uint64_t new_capacity);
bool itable_contains (struct itable *table, uint64_t key);

#define for_each_itable_entry(table, varname) \
	for (struct itable_entry *varname = (table)->head; varname; varname = varname->next)

#endif /* FHTTPD_ITABLE_H */
