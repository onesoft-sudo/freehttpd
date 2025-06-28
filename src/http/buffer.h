#ifndef FH_BUFFER_H
#define FH_BUFFER_H

#include <stdint.h>
#include <stddef.h>

#include "types.h"

enum fh_buf_type
{
	FH_BUF_DATA,
	FH_BUF_REF,
	FH_BUF_FILE
};

union fh_buf_payload
{
	char *data;
	char *ref;
	fd_t fd;
};

struct fh_buf
{
	enum fh_buf_type type;
	size_t size;
	union fh_buf_payload payload;
};

#endif /* FH_BUFFER_H */
