#ifndef FH_POOL_H
#define FH_POOL_H

#include <stddef.h>

struct fh_pool;

void *fh_pool_alloc (struct fh_pool *pool, size_t size);
void *fh_pool_calloc (struct fh_pool *pool, size_t n, size_t size);
void *fh_pool_zalloc (struct fh_pool *pool, size_t size);
void *fh_pool_realloc (struct fh_pool *pool, void *ptr, size_t new_size);
struct fh_pool *fh_pool_create_child (struct fh_pool *pool, size_t init_cap);
bool fh_pool_attach (struct fh_pool *pool, void *ptr);

struct fh_pool *fh_pool_create (size_t init_cap);
void fh_pool_destroy (struct fh_pool *pool);

#endif /* FH_POOL_H */
