#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FHTTPD_LOG_MODULE_NAME "pool"

#include "log/log.h"
#include "pool.h"

#define FH_POOL_DEFAULT_CAP (1024 * 8)
#define FH_POOL_CHUNK_SIZE (1024 * 8)
#define FH_POOL_DEFAULT_SIG 0b1101010
#define FH_POOL_ATTACH_CAP_INCREMENT 128
#define FH_POOL_CHILD_CAP_INCREMENT 32
#define FH_POOL_ALLOC_LARGE_MIN_SIZE 4096

static uint64_t next_pool_id = 0;

struct fh_pool_chunk
{
	size_t cap, off;
	struct fh_pool_chunk *next;
	uint8_t space[];
};

struct fh_pool_ptr
{
	void *ptr;
	void (*cleanup_cb) (void *);
};

struct fh_pool
{
	uint64_t id;

	struct fh_pool_chunk *head;
	size_t chunk_count;

	struct fh_pool_ptr *attached_ptrs;
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

	init_cap = init_cap ? init_cap : FH_POOL_DEFAULT_CAP;

	memset (pool, 0, sizeof (*pool));
	pool->chunk_count = 1;
	pool->head = malloc (sizeof (struct fh_pool_chunk) + init_cap);

	if (!pool->head)
	{
		free (pool);
		return NULL;
	}

	pool->head->cap = pool->head->off = 0;
	pool->head->next = NULL;
	pool->id = next_pool_id++;

	return pool;
}

void
fh_pool_destroy (struct fh_pool *pool)
{
	fhttpd_wclog_debug ("Pool #%lu: Deallocating", pool->id);

	for (size_t i = 0; i < pool->child_count; i++)
	{
		fhttpd_wclog_debug ("Pool #%lu: Child pool #%lu is being deallocated", pool->id, pool->children[i]->id);
		fh_pool_destroy (pool->children[i]);
	}

	free (pool->children);

	fhttpd_wclog_debug ("Pool #%lu: Pointers to free: %zu", pool->id, pool->attached_ptr_count);

	for (size_t i = 0; i < pool->attached_ptr_count; i++)
	{
		struct fh_pool_ptr *ptr_data = &pool->attached_ptrs[i];
		void *ptr = ptr_data->ptr;

		if (!ptr)
			continue;

		fhttpd_wclog_debug ("Pool #%lu: Freeing pointer: %p", pool->id, ptr);

		if (ptr_data->cleanup_cb)
			ptr_data->cleanup_cb (ptr);
		else
			free (ptr_data->ptr);
	}

	free (pool->attached_ptrs);

	for (struct fh_pool_chunk *c = pool->head; c;)
	{
		struct fh_pool_chunk *c_free = c;
		c = c->next;
		free (c_free);
	}

	free (pool);
}

static bool
fh_pool_grow (struct fh_pool *pool, size_t min_size)
{
	min_size = min_size < FH_POOL_CHUNK_SIZE ? FH_POOL_CHUNK_SIZE : min_size;
	struct fh_pool_chunk *c = malloc (sizeof (struct fh_pool_chunk) + min_size);

	if (!c)
		return false;

	c->cap = c->off = 0;
	pool->chunk_count++;

	if (!pool->head)
	{
		pool->head = c;
		c->next = NULL;
	}
	else
	{
		c->next = pool->head;
		pool->head = c;
	}

	return true;
}

bool
fh_pool_cancel_last_alloc (struct fh_pool *pool, size_t size)
{
	struct fh_pool_chunk *c = pool->head;

	if (size > c->off)
		return false;

	fhttpd_wclog_debug ("Space returned: %zu bytes", size);
	c->off -= size;
	return true;
}

void *
fh_pool_alloc (struct fh_pool *pool, size_t size)
{
	if (size >= FH_POOL_ALLOC_LARGE_MIN_SIZE)
	{
		void *ptr = malloc (size);

		if (!ptr)
			return NULL;

		if (!fh_pool_attach (pool, ptr, NULL))
		{
			free (ptr);
			return NULL;
		}

		return ptr;
	}

	struct fh_pool_chunk *c = pool->head;

	if (c->off + size >= c->cap)
	{
		if (!fh_pool_grow (pool, size))
			return NULL;

		c = pool->head;
	}

	void *ptr = (void *) (((char *) c->space) + c->off);
	c->off += size;
	return ptr;
}

void *
fh_pool_calloc (struct fh_pool *pool, size_t n, size_t size)
{
	if ((n * size) >= FH_POOL_ALLOC_LARGE_MIN_SIZE)
	{
		void *ptr = calloc (n, size);

		if (!ptr)
			return NULL;

		if (!fh_pool_attach (pool, ptr, NULL))
		{
			free (ptr);
			return NULL;
		}

		return ptr;
	}

	return fh_pool_zalloc (pool, n * size);
}

void *
fh_pool_zalloc (struct fh_pool *pool, size_t size)
{
	if (size >= FH_POOL_ALLOC_LARGE_MIN_SIZE)
	{
		void *ptr = calloc (1, size);

		if (!ptr)
			return NULL;

		if (!fh_pool_attach (pool, ptr, NULL))
		{
			free (ptr);
			return NULL;
		}

		return ptr;
	}

	void *ptr = fh_pool_alloc (pool, size);

	if (!ptr)
		return NULL;

	memset (ptr, 0, size);
	return ptr;
}

bool
fh_pool_attach (struct fh_pool *pool, void *ptr, void (*cleanup_cb) (void *))
{
	if (pool->attached_ptr_count >= pool->attached_ptr_cap)
	{
		struct fh_pool_ptr *ptrs = realloc (
			pool->attached_ptrs, sizeof (struct fh_pool_ptr) * (pool->attached_ptr_cap + FH_POOL_ATTACH_CAP_INCREMENT));

		if (!ptrs)
			return false;

		pool->attached_ptrs = ptrs;
		pool->attached_ptr_cap += FH_POOL_ATTACH_CAP_INCREMENT;
	}

	pool->attached_ptrs[pool->attached_ptr_count].ptr = ptr;
	pool->attached_ptrs[pool->attached_ptr_count].cleanup_cb = cleanup_cb;

	pool->attached_ptr_count++;
	return true;
}

struct fh_pool *
fh_pool_create_child (struct fh_pool *pool, size_t init_cap)
{
	if (pool->child_count >= pool->child_cap)
	{
		struct fh_pool **children
			= realloc (pool->children, sizeof (struct fh_pool *) * (pool->child_cap + FH_POOL_CHILD_CAP_INCREMENT));

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
