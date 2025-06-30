#ifndef FH_BUFFER_H
#define FH_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#include "types.h"

enum fh_buf_type
{
	FH_BUF_DATA,
	FH_BUF_FILE
};

struct fh_buf_file
{
	fd_t fd;
	off_t start, end;
};

struct fh_buf_data
{
	uint8_t *start;
	size_t len;
	bool is_readonly;
};

union fh_buf_payload
{
	struct fh_buf_data data;
	struct fh_buf_file file;
};

struct fh_buf
{
	enum fh_buf_type type;
	union fh_buf_payload payload;
};

void fh_buf_dump (const struct fh_buf *buf);

#endif /* FH_BUFFER_H */
