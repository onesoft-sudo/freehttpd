#ifndef FH_STREAM_H
#define FH_STREAM_H

#include <stdbool.h>
#include <stddef.h>

#include "mm/pool.h"
#include "buffer.h"

struct fh_chain
{
	struct fh_buf   *buf;
	struct fh_chain *next;
	bool is_eos   : 1;
	bool is_start : 1;
};

struct fh_stream
{
	struct fh_pool *pool;
	struct fh_chain *head;
	struct fh_chain *tail;
	size_t length;
	size_t size_total;
	bool can_be_large;
};

struct fh_stream *fh_stream_create (struct fh_pool *pool);
struct fh_chain *fh_chain_new (struct fh_pool *pool);
struct fh_chain *fh_stream_append_chain_memcpy (struct fh_stream *stream, const uint8_t *src, size_t len);
struct fh_chain *fh_stream_append_chain_data (struct fh_stream *stream, uint8_t *src, size_t len);

void fh_stream_dump (struct fh_stream *stream);

#define fh_stream_for_each(stream, varname) \
	for (struct fh_chain *varname = (stream)->head; varname; varname = varname->next)

#endif /* FH_STREAM_H */
