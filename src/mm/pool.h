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
