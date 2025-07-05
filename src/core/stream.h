#ifndef FH_CORE_STREAM_H
#define FH_CORE_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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
			size_t file_start;
			size_t file_len;
		};

		struct {
			bool rd_only : 1;
			uint8_t *data;
			size_t len;
			size_t cap;
		};
	};
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
struct fh_buf *fh_stream_alloc_buf_data (struct fh_stream *stream, size_t cap);
struct fh_buf *fh_stream_add_buf_data (struct fh_stream *stream, uint8_t *src, size_t len, size_t cap);
struct fh_buf *fh_stream_add_buf_memcpy (struct fh_stream *stream, const uint8_t *src, size_t len, size_t cap);

#endif /* FH_CORE_STREAM_H */
