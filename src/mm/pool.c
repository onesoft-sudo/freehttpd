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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "pool.h"

#undef fh_pool_alloc
#undef fh_pool_zalloc
#undef fh_pool_undo_last_alloc

struct fh_pool *
fh_pool_create (size_t init_cap)
{
	init_cap = init_cap ? init_cap : FH_DEFAULT_CHUNK_CAP;
	struct fh_pool *pool = zalloc (sizeof (*pool) + sizeof (*pool->current) + init_cap);

	if (!pool)
		return NULL;

	pool->current = (struct fh_pool_chunk *) (pool + 1);
	pool->current->mptr = (void *) (pool->current + 1);
	pool->current->cap = init_cap;
	pool->current->non_freeable = true;
	pool->chunk_count = 1;

	return pool;
}

void
fh_pool_destroy (struct fh_pool *pool)
{
	struct fh_pool_chunk *c = pool->current;

	while (c)
	{
		struct fh_pool_chunk *next = c->next;

		if (!c->non_freeable)
			free (c);

		c = next;
	}

	struct fh_pool_malloc *m = pool->mallocs;

	while (m)
	{
		struct fh_pool_malloc *next = m->next;

		if (m->cleanup_cb)
			m->cleanup_cb (m->mptr);
		
		free (m->mptr);
		m = next;
	}

	struct fh_pool *p = pool->last_child;

	while (p)
	{
		struct fh_pool *next = p->next;
		fh_pool_destroy (p);
		p = next;
	}

	free (pool);
}

void *
fh_pool_large_alloc (struct fh_pool *pool, size_t size, fh_pool_cleanup_cb_t cleanup_cb)
{
	void *mem = malloc (sizeof (struct fh_pool_malloc) + size);

	if (!mem)
		return NULL;

	struct fh_pool_malloc *m = (struct fh_pool_malloc *) (((char *) (mem)) + size);

	m->cleanup_cb = cleanup_cb;
	m->mptr = mem;
	m->size = size;
	m->next = pool->mallocs;

	pool->mallocs = m;
	pool->malloc_count++;

	return m->mptr;
}

void *
fh_pool_alloc (struct fh_pool *pool, size_t size)
{
	if (size > FH_SMALL_MAX_SIZE)
		return fh_pool_large_alloc (pool, size, NULL);

	if (pool->current->used + size > pool->current->cap)
	{
		const size_t cap = size >= FH_DEFAULT_CHUNK_CAP ? size : FH_DEFAULT_CHUNK_CAP;
		struct fh_pool_chunk *c = malloc (sizeof (*c) + cap);

		if (!c)
			return NULL;

		c->cap = cap;
		c->used = size;
		c->mptr = (void *) (c + 1);
		c->non_freeable = false;
		c->next = pool->current;

		pool->current = c;
		pool->chunk_count++;

		return c->mptr;
	}

	void *mptr = (void *) (((uint8_t *) pool->current->mptr) + pool->current->used);
	pool->current->used += size;
	return mptr;
}

struct fh_pool *
fh_pool_create_child (struct fh_pool *pool, size_t init_cap)
{
	struct fh_pool *child = fh_pool_create (init_cap);

	if (!child)
		return NULL;

	if (!pool->last_child)
	{
		pool->last_child = child;
		pool->child_count = 1;
	}
	else
	{
		pool->last_child->next = child;
		pool->last_child = child;
		pool->child_count++;
	}

	return child;
}
