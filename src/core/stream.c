#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void
fh_stream_append_chain (struct fh_stream *stream, struct fh_chain *chain)
{
	if (!stream->tail)
	{
        stream->head = chain;
		stream->tail = chain;
		stream->length = 1;
		return;
	}

    stream->tail->next = chain;
    stream->tail = chain;
	stream->length++;
    chain->next = NULL;
}

struct fh_chain *
fh_chain_new (struct fh_pool *pool)
{
	struct fh_chain *chain = fh_pool_zalloc (pool, sizeof (struct fh_chain));

	if (!chain)
		return NULL;

	return chain;
}

struct fh_chain *
fh_stream_append_chain_memcpy (struct fh_stream *stream, const uint8_t *src, size_t len)
{
	struct fh_chain *chain = fh_pool_zalloc (stream->pool, sizeof (struct fh_chain));

	if (!chain)
		return NULL;

	chain->buf = fh_pool_alloc (stream->pool, sizeof (*chain->buf));

	if (!chain->buf)
		return NULL;

	chain->buf->type = FH_BUF_DATA;
	chain->buf->payload.data.start = fh_pool_alloc (stream->pool, len);

	if (!chain->buf->payload.data.start)
		return NULL;

	chain->buf->payload.data.is_readonly = false;
	chain->buf->payload.data.len = len;
	stream->size_total += len;

	memcpy (chain->buf->payload.data.start, src, len);
	fh_stream_append_chain (stream, chain);
	return chain;
}

struct fh_chain *
fh_stream_append_chain_data (struct fh_stream *stream, uint8_t *src, size_t len)
{
	struct fh_chain *chain = fh_pool_zalloc (stream->pool, sizeof (struct fh_chain));

	if (!chain)
		return NULL;

	chain->buf = fh_pool_alloc (stream->pool, sizeof (*chain->buf));

	if (!chain->buf)
		return NULL;

	chain->buf->type = FH_BUF_DATA;
	chain->buf->payload.data.is_readonly = false;
	chain->buf->payload.data.len = len;
	stream->size_total += len;
	chain->buf->payload.data.start = src;

	fh_stream_append_chain (stream, chain);
	return chain;
}

void
fh_chain_dump (struct fh_chain *chain)
{
	printf ("Chain <%p>:\n", (void *) chain);
	printf ("Flags: ");

	if (chain->is_eos)
		printf ("eos ");
	
	if (chain->is_start)
		printf ("start ");

	printf ("\nNext: %p\n", (void *) chain->next);
	fh_buf_dump (chain->buf);
}

void
fh_stream_dump (struct fh_stream *stream)
{
	printf ("Stream <%p>:\n", (void *) stream);
	printf ("Length:     %zu\n\n", stream->length);
	printf ("Size total: %zu\n\n", stream->size_total);

	fh_stream_for_each (stream, chain) {
		fh_chain_dump (chain);
	}
}