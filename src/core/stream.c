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

#include <stdlib.h>

#define FH_LOG_MODULE_NAME "stream"

#include "log/log.h"
#include "mm/pool.h"
#include "stream.h"

struct fh_stream *
fh_stream_new (pool_t *pool)
{
	struct fh_stream *stream = fh_pool_zalloc (pool, sizeof (*stream));

	if (!stream)
		return NULL;

	stream->pool = pool;
	return stream;
}

void
fh_stream_init (struct fh_stream *stream, pool_t *pool)
{
	stream->pool = pool;
	stream->head = stream->tail = NULL;
	stream->len = 0;
}

static inline void
fh_stream_insert (struct fh_stream *stream, struct fh_link *link)
{
	if (!stream->tail)
	{
		stream->head = stream->tail = link;
		return;
	}

	stream->tail->next = link;
	stream->tail = link;
}

struct fh_buf *
fh_stream_alloc_buf_data (struct fh_stream *stream, size_t cap)
{
	bool is_big = cap > FH_SMALL_MAX_SIZE;
	struct fh_link *l = fh_pool_alloc (stream->pool, sizeof (*l) + sizeof (*l->buf) + (is_big ? 0 : cap));

	if (!l)
		return NULL;

	l->next = NULL;
	l->buf = (struct fh_buf *) (l + 1);
	l->buf->data = is_big ? fh_pool_alloc (stream->pool, cap) : ((uint8_t *) (l->buf + 1));

	if (!l->buf->data)
		return NULL;

	l->buf->type = FH_BUF_DATA;
	l->buf->cap = cap;
	l->is_eos = l->is_start = false;

	fh_stream_insert (stream, l);
	return l->buf;
}

struct fh_buf *
fh_stream_add_buf_data (struct fh_stream *stream, uint8_t *src, size_t len, size_t cap)
{
	cap = cap >= len ? cap : len;

	struct fh_link *l = fh_pool_alloc (stream->pool, sizeof (*l) + sizeof (*l->buf));

	if (!l)
		return NULL;

	l->next = NULL;
	l->buf = (struct fh_buf *) (l + 1);
	l->buf->rd_only = true;
	l->buf->data = src;
	l->buf->len = len;
	l->buf->cap = cap;
	l->is_eos = l->is_start = false;

	fh_stream_insert (stream, l);
	return l->buf;
}

struct fh_buf *
fh_stream_add_buf_memcpy (struct fh_stream *stream, const uint8_t *src, size_t len, size_t cap)
{
	cap = cap >= len ? cap : len;

	struct fh_buf *buf = fh_stream_alloc_buf_data (stream, cap);

	if (!buf)
		return NULL;

	buf->len = len;
	memcpy (buf->data, src, len);

	return buf;
}

void
fh_stream_print (struct fh_stream *stream)
{
	struct fh_link *l = stream->head;
	size_t i = 0;

	fh_pr_debug ("Links: %zu", stream->len);

	while (l)
	{
		fh_pr_debug ("=======");
		fh_pr_debug ("Buffer #%zu", i++);
		fh_pr_debug ("Length: %zu/%zu bytes", l->buf->len, l->buf->cap);
		fh_pr_debug ("Start: %u", l->is_start);
		fh_pr_debug ("EOS: %u", l->is_eos);
		fh_pr_debug ("Readonly: %u", l->buf->rd_only);
		fh_pr_debug ("Data: |%.*s|", (int) l->buf->len, l->buf->data);
		fh_pr_debug ("=======");
		l = l->next;
	}
}

size_t
fh_stream_copy (void *dest, struct fh_link *start, size_t start_off, struct fh_link *end, size_t end_off,
				size_t max_size)
{
	size_t copied = 0;
	struct fh_link *link = start;

	while (link && copied < max_size)
	{
		size_t len = 0;

		if (start == end)
		{
			size_t s = end_off - start_off + 1;
			len = (s > link->buf->len) ? link->buf->len : s;
		}
		else if (link == start)
		{
			len = link->buf->len - start_off;
		}
		else if (link == end)
		{
			size_t s = end_off + 1;
			len = (s > link->buf->len) ? link->buf->len : s;
		}
		else
		{
			len = link->buf->len;
		}

		if (copied + len > max_size)
			len = max_size - copied;

		memcpy (((char *) dest) + copied, link->buf->data + start_off, len);
		copied += len;

		start_off = 0;
		link = link->next;
	}

	return copied;
}
