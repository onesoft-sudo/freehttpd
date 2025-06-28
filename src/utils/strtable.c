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

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strtable.h"

struct strtable *
strtable_create (size_t capacity)
{
	capacity = capacity > 0 ? capacity : STRTABLE_DEFAULT_CAPACITY;

	struct strtable *table = calloc (1, sizeof (struct strtable));

	if (!table)
		return NULL;

	table->capacity = capacity;
	table->count = 0;
	table->head = NULL;
	table->tail = NULL;
	table->buckets = calloc (capacity, sizeof (struct strtable_entry));

	if (!table->buckets)
	{
		free (table);
		return NULL;
	}

	return table;
}

void
strtable_destroy (struct strtable *table)
{
	if (!table)
		return;

	struct strtable_entry *e = table->head;

	while (e)
	{
		free (e->key);
		e = e->next;
	}

	free (table->buckets);
	free (table);
}

static size_t
htable_hash_fnv1a (const char *key, size_t capacity)
{
	const uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325;
	const uint64_t FNV_PRIME = 0x100000001b3;
	uint64_t hash = FNV_OFFSET_BASIS;

	while (*key)
	{
		hash ^= (uint8_t) *key;
		hash *= FNV_PRIME;
		key++;
	}

	return hash % capacity;
}

void *
strtable_get (struct strtable *table, const char *key)
{
	key = key == NULL ? "" : key;

	size_t hash = htable_hash_fnv1a (key, table->capacity);
	struct strtable_entry *entry = &table->buckets[hash];
	bool first_iteration = true;

	while (entry)
	{
		if (entry->key && !strcmp (entry->key, key))
		{
			return entry->data;
		}
		if (!first_iteration && !entry->next)
			break;

		entry = entry->next;

		if (entry == NULL)
		{
			entry = table->head;
			first_iteration = false;
		}
	}

	return NULL;
}

bool
strtable_set (struct strtable *table, const char *key, void *data)
{
	if (table->count >= ((table->capacity * 75) / 100)
		&& !strtable_resize (table,
							 table->capacity >= 1024 * 1024 ? table->capacity + 1024 * 1024 : table->capacity * 2))
	{
#ifndef NDEBUG
		size_t newcap = table->capacity >= 1024 * 1024 ? table->capacity + 1024 * 1024 : table->capacity * 2;
		fprintf (stderr, "%s: Failed to resize hash table for key %s\n", __func__, key);
		fprintf (stderr, "Current capacity: %zu, count: %zu, new capacity: %zu\n", table->capacity, table->count,
				 newcap);
#endif

		return false;
	}

	key = key == NULL ? "" : key;

	size_t hash = htable_hash_fnv1a (key, table->capacity);

	for (; hash < table->capacity; hash++)
	{
		struct strtable_entry *entry = &table->buckets[hash];

		if (entry->key && !strcmp (entry->key, key))
		{
			entry->data = data;
			return true;
		}

		if (entry->key == NULL)
		{
			entry->key = strdup (key);
			entry->data = data;

			entry->next = NULL;
			entry->prev = table->tail;

			if (table->tail)
				table->tail->next = entry;
			else
				table->head = entry;

			table->tail = entry;
			table->count++;

			return true;
		}
	}

#ifndef NDEBUG
	fprintf (stderr, "%s: Hash table is full, cannot insert key %s\n", __func__, key);
#endif

	return false;
}

void *
strtable_remove (struct strtable *table, const char *key)
{
	if (table->count == 0)
		return NULL;

	key = key == NULL ? "" : key;

	size_t hash = htable_hash_fnv1a (key, table->capacity);
	struct strtable_entry *entry = &table->buckets[hash];
	bool first_iteration = true;

	while (entry)
	{
		if (entry->key && !strcmp (entry->key, key))
		{
			void *data = entry->data;

			free (entry->key);

			entry->key = NULL;
			entry->data = NULL;

			if (entry->prev)
				entry->prev->next = entry->next;
			else
				table->head = entry->next;

			if (entry->next)
				entry->next->prev = entry->prev;
			else
				table->tail = entry->prev;

			entry->next = NULL;
			entry->prev = NULL;
			table->count--;

			return data;
		}

		if (!first_iteration && entry == &table->buckets[hash])
			break;

		entry = entry->next;

		if (entry == NULL)
		{
			entry = table->head;
			first_iteration = false;
		}
	}

	return NULL;
}

bool
strtable_resize (struct strtable *table, size_t new_capacity)
{
	if (new_capacity <= table->capacity)
	{
		return false;
	}

	struct strtable_entry *new_buckets = calloc (new_capacity, sizeof (struct strtable_entry));

	if (!new_buckets)
	{
		return false;
	}

	struct strtable_entry *head = table->head;
	struct strtable_entry *new_head = NULL, *new_tail = NULL;

	while (head)
	{
		size_t new_hash = htable_hash_fnv1a (head->key, new_capacity);

		for (; new_hash < new_capacity; new_hash++)
		{
			struct strtable_entry *entry = &new_buckets[new_hash];

			if (entry->key == NULL)
			{
				entry->key = head->key;
				entry->data = head->data;

				entry->next = NULL;
				entry->prev = new_tail;

				if (new_tail)
					new_tail->next = entry;
				else
					new_head = entry;

				new_tail = entry;

#ifndef NDEBUG
				printf ("Moved key %s to new bucket %zu\n", head->key, new_hash);
#endif
				break;
			}
		}

		head = head->next;
	}

	free (table->buckets);

	table->head = new_head;
	table->tail = new_tail;
	table->buckets = new_buckets;
	table->capacity = new_capacity;

	return true;
}

bool
strtable_contains (struct strtable *table, const char *key)
{
	if (!table || table->count == 0)
		return false;

	key = key == NULL ? "" : key;

	size_t hash = htable_hash_fnv1a (key, table->capacity);
	struct strtable_entry *entry = &table->buckets[hash];
	bool first_iteration = true;

	while (entry)
	{
		if (entry->key && !strcmp (entry->key, key))
			return true;

		if (!first_iteration && !entry->next)
			break;

		entry = entry->next;

		if (entry == NULL)
		{
			entry = table->head;
			first_iteration = false;
		}
	}

	return false;
}
