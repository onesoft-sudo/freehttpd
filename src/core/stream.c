#include <stdlib.h>

#include "stream.h"

struct fh_stream *
fh_stream_create (struct fh_pool *pool)
{
	struct fh_stream *stream = fh_pool_zalloc (pool, sizeof (*stream));

	if (!stream)
		return NULL;

	stream->pool = pool;
	return stream;
}

bool
fh_stream_append (struct fh_stream *stream, struct fh_chain *chain)
{
	if (!stream->tail)
	{
        stream->head = chain;
		stream->tail = chain;
        return true;
	}

    stream->tail->next = chain;
    stream->tail = chain;
    chain->next = NULL;
    return true;
}
