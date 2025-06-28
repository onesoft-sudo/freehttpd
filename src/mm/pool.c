#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pool.h"

#define FH_POOL_DEFAULT_CAP          (1024 * 512)
#define FH_POOL_MIN_GROW_SIZE        (1024 * 256)
#define FH_POOL_DEFAULT_SIG          0b1101010
#define FH_POOL_ATTACH_CAP_INCREMENT 128
#define FH_POOL_CHILD_CAP_INCREMENT  32

struct fh_pool
{
	void *space;
	size_t cap;
	size_t off;

	void **attached_ptrs;
	size_t attached_ptr_count;
	size_t attached_ptr_cap;

	struct fh_pool **children;
	size_t child_count;
	size_t child_cap;
};

struct fh_pool *
fh_pool_create (size_t init_cap)
{
	struct fh_pool *pool = malloc (sizeof (*pool));

	if (!pool)
		return NULL;

	memset (pool, 0, sizeof (*pool));
	pool->cap = init_cap ? init_cap : FH_POOL_DEFAULT_CAP;
	pool->space = malloc (pool->cap);

	if (!pool->space)
	{
		free (pool);
		return NULL;
	}

	return pool;
}

void
fh_pool_destroy (struct fh_pool *pool)
{
	for (size_t i = 0; i < pool->child_count; i++)
		fh_pool_destroy (pool->children[i]);

	for (size_t i = 0; i < pool->attached_ptr_count; i++)
		free (pool->attached_ptrs[i]);

	free (pool->children);
	free (pool->attached_ptrs);
	free (pool->space);
	free (pool);
}

static bool
fh_pool_grow (struct fh_pool *pool, size_t min_size)
{
	min_size = min_size < FH_POOL_MIN_GROW_SIZE
		? FH_POOL_MIN_GROW_SIZE : min_size;
	void *space = realloc (pool->space, pool->cap + min_size);

	if (!space)
		return false;

	pool->space = space;
	pool->cap += min_size;
	return true;
}

void *
fh_pool_alloc (struct fh_pool *pool, size_t size)
{
	if (pool->off + size >= pool->cap && !fh_pool_grow (pool, size))
		return NULL;

	void *ptr = (void *) (((char *) pool->space) + pool->off);
	pool->off += size;
	return ptr;
}

void *
fh_pool_calloc (struct fh_pool *pool, size_t n, size_t size)
{
	return fh_pool_zalloc (pool, n * size);
}

void *
fh_pool_zalloc (struct fh_pool *pool, size_t size)
{
	void *ptr = fh_pool_alloc (pool, size);

	if (!ptr)
		return NULL;

	memset (ptr, 0, size);
	return ptr;
}

bool
fh_pool_attach (struct fh_pool *pool, void *ptr)
{
	if (pool->attached_ptr_count >= pool->attached_ptr_cap)
	{
		void **ptrs = realloc (pool->attached_ptrs, sizeof (void *) *
							   (pool->attached_ptr_cap + FH_POOL_ATTACH_CAP_INCREMENT));

		if (!ptrs)
			return false;

		pool->attached_ptrs = ptrs;
		pool->attached_ptr_cap += FH_POOL_ATTACH_CAP_INCREMENT;
	}

	pool->attached_ptrs[pool->attached_ptr_count++] = ptr;
	return true;
}

struct fh_pool *
fh_pool_create_child (struct fh_pool *pool, size_t init_cap)
{
	if (pool->child_count >= pool->child_cap)
	{
		struct fh_pool **children = realloc (pool->children, sizeof (struct fh_pool *) *
							   (pool->child_cap + FH_POOL_CHILD_CAP_INCREMENT));

		if (!children)
			return false;

		pool->children = children;
		pool->child_cap += FH_POOL_CHILD_CAP_INCREMENT;
	}

	struct fh_pool *child = fh_pool_create (init_cap);

	if (!child)
		return NULL;

	pool->children[pool->child_count++] = child;
	return child;
}
