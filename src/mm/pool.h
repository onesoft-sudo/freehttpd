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

#ifndef FH_POOL_H
#define FH_POOL_H

#include <stdbool.h>
#include <stddef.h>

/* Defines a pointer declaration that might not be const,
   but cannot be free'd.  That means, the the pointer can be 
   used to write to memory, but it cannot be free'd. */
#define _nonfreeable

struct fh_pool;

void *fh_pool_alloc (struct fh_pool *pool, size_t size);
void *fh_pool_calloc (struct fh_pool *pool, size_t n, size_t size);
void *fh_pool_zalloc (struct fh_pool *pool, size_t size);
struct fh_pool *fh_pool_create_child (struct fh_pool *pool, size_t init_cap);
bool fh_pool_attach (struct fh_pool *pool, void *ptr, void (*cleanup_cb)(void *));

struct fh_pool *fh_pool_create (size_t init_cap);
void fh_pool_destroy (struct fh_pool *pool);

bool fh_pool_cancel_last_alloc (struct fh_pool *pool, size_t size);

#endif /* FH_POOL_H */
