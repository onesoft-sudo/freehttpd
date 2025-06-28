#ifndef FH_STREAM_H
#define FH_STREAM_H

#include <stdbool.h>
#include <stddef.h>

#include "mm/pool.h"
#include "buffer.h"

struct fh_chain
{
	struct fh_buffer *buffer;
	struct fh_chain *next;
	bool is_eos   : 1;
	bool is_start : 1;
};

struct fh_stream
{
	struct fh_pool *pool;
	struct fh_chain *head;
	size_t length;
};

struct fh_stream *fh_stream_create (struct fh_pool *pool);
bool fh_stream_append (struct fh_stream *stream, struct fh_chain *chain);

#define fh_stream_for_each (stream, varname) \
	for (struct fh_chain *varname = (stream)->head; varname; varname = varname->next)

#endif /* FH_STREAM_H */
