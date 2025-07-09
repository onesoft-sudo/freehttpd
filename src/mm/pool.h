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

#ifndef FH_MM_POOL_H
#define FH_MM_POOL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#define FH_SMALL_MAX_SIZE 4096
#define FH_DEFAULT_CHUNK_CAP 8192

typedef void (*fh_pool_cleanup_cb_t) (void *);

struct fh_pool_chunk
{
	void *mptr;
	size_t cap, used;
	struct fh_pool_chunk *next;
	bool non_freeable : 1;
};

struct fh_pool_malloc
{
	void *mptr;
	size_t size;
	fh_pool_cleanup_cb_t cleanup_cb;
	struct fh_pool_malloc *next;
};

struct fh_pool
{
	struct fh_pool_chunk *current;
	struct fh_pool_malloc *mallocs;
	size_t chunk_count, malloc_count;
	struct fh_pool *last_child;
	size_t child_count;
	struct fh_pool *next;
};

typedef struct fh_pool pool_t;

#define fh_pool_alloc(a, b) malloc (b)
#define fh_pool_zalloc(a, b) calloc (1, b)
#define fh_pool_undo_last_alloc(...) NULL

struct fh_pool *fh_pool_create (size_t init_cap);
void fh_pool_destroy (struct fh_pool *pool);
void *fh_pool_large_alloc (struct fh_pool *pool, size_t size, fh_pool_cleanup_cb_t cleanup_cb);
struct fh_pool *fh_pool_create_child (struct fh_pool *pool, size_t init_cap);
// void *fh_pool_alloc (struct fh_pool *pool, size_t size);

// __attribute__ ((always_inline)) __attribute_maybe_unused__ static inline void
// fh_pool_undo_last_alloc (struct fh_pool *pool, size_t size)
// {
// 	pool->current->used -= size;
// }

// __attribute__ ((always_inline)) __attribute_maybe_unused__ static inline void *
// fh_pool_calloc (struct fh_pool *pool, size_t n, size_t size)
// {
// 	size *= n;

// 	void *mptr = fh_pool_alloc (pool, size);

// 	if (!mptr)
// 		return NULL;

// 	memset (mptr, 0, size);
// 	return mptr;
// }

// __attribute__ ((always_inline)) __attribute_maybe_unused__ static inline void *
// fh_pool_zalloc (struct fh_pool *pool, size_t size)
// {
// 	return fh_pool_calloc (pool, 1, size);
// }

#endif /* FH_MM_POOL_H */
