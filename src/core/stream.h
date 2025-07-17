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

#ifndef FH_CORE_STREAM_H
#define FH_CORE_STREAM_H

#define _GNU_SOURCE

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#undef _GNU_SOURCE

#include "types.h"
#include "mm/pool.h"

enum fh_buf_type
{
	FH_BUF_DATA,
	FH_BUF_FILE
};

struct fh_buf
{
	uint8_t type;
	bool freeable : 1;

	union {
		struct {
			fd_t file_fd;
			size_t file_off;
			size_t file_len;
		} file;

		struct {
			bool rd_only : 1;
			uint8_t *data;
			size_t len;
			size_t cap;
		} mem;
	} attrs;
};

struct fh_link
{
	struct fh_buf *buf;
	struct fh_link *next;
	bool is_eos : 1;
	bool is_start : 1;
};

struct fh_stream
{
	pool_t *pool;
	struct fh_link *head, *tail;
	size_t len;
};

struct fh_stream *fh_stream_new (pool_t *pool);
void fh_stream_init (struct fh_stream *stream, pool_t *pool);
struct fh_buf *fh_stream_alloc_buf_data (struct fh_stream *stream, size_t cap);
struct fh_buf *fh_stream_add_buf_data (struct fh_stream *stream, uint8_t *src, size_t len, size_t cap);
struct fh_buf *fh_stream_add_buf_memcpy (struct fh_stream *stream, const uint8_t *src, size_t len, size_t cap);
size_t fh_stream_copy (void *dest, struct fh_link *start, size_t start_off, struct fh_link *end, size_t end_off, size_t max_size);
void fh_stream_print (struct fh_stream *stream);

#endif /* FH_CORE_STREAM_H */
