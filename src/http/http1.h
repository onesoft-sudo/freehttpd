#ifndef FH_HTTP1_H
#define FH_HTTP1_H

#include <stddef.h>
#include "core/stream.h"

struct fh_http1_cursor
{
	struct fh_link *link;
	size_t off;
};

#endif /* FH_HTTP1_H */