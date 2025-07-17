#ifndef FH_MODULES_AUTOINDEX_H
#define FH_MODULES_AUTOINDEX_H

#include <stddef.h>
#include <stdint.h>

#include "core/conn.h"
#include "core/stream.h"
#include "router/filesystem.h"
#include "http/http1_request.h"
#include "http/http1_response.h"
#include "router/router.h"
#include "utils/utils.h"

struct fh_autoindex
{
	struct fh_router *router;
	struct fh_conn *conn;
	const struct fh_request *request;
	struct fh_response *response;
	const char *filename;
	size_t filename_len;
	const struct stat64 *st;
};

bool fh_autoindex_handle (struct fh_autoindex *autoindex);

#endif /* FH_MODULES_AUTOINDEX_H */
