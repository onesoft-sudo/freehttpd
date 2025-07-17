#define _GNU_SOURCE

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "router.h"
#include "utils/utils.h"
#include "filesystem.h"
#include "core/stream.h"
#include "core/conn.h"
#include "http/http1_request.h"
#include "http/http1_response.h"

static bool
fh_router_handle_static_file (struct fh_router *router, struct fh_conn *conn,
							 const struct fh_request *request,
							 struct fh_response *response, fd_t fd, const struct stat64 *st)
{
    (void) router;
    (void) request;
    (void) conn;

    response->body_start = fh_pool_alloc (
		response->pool, sizeof (struct fh_link) + sizeof (struct fh_buf));

	if (!response->body_start)
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		close (fd);
		return true;
	}

	response->body_start->buf = (struct fh_buf *) (response->body_start + 1);
	struct fh_buf *buf = response->body_start->buf;

	buf->type = FH_BUF_FILE;
	buf->attrs.file.file_fd = fd;
	buf->attrs.file.file_off = 0;
	buf->attrs.file.file_len = st->st_size;

	response->body_start->next = NULL;
	response->body_start->is_eos = true;
	response->content_length = st->st_size;
	response->use_default_error_response = false;
	response->status = FH_STATUS_OK;

	fh_pr_debug ("Successfully generated response");
    return true;
}

bool
fh_router_handle_filesystem (struct fh_router *router, struct fh_conn *conn,
							 const struct fh_request *request,
							 struct fh_response *response)
{
	(void) router;

	char path_buf[PATH_MAX + 1] = { 0 };
	char normalized_path[PATH_MAX + 1] = { 0 };
	int path_buf_len = 0;
	size_t normalized_path_len = 0;

	response->use_default_error_response = true;

	if (request->uri_len >= INT32_MAX
		|| (path_buf_len = snprintf (path_buf, sizeof path_buf, "%s/%.*s",
									 conn->config->host_config->docroot,
									 (int) request->uri_len, request->uri))
			   >= PATH_MAX
		|| path_buf_len < 0)
	{
		fh_pr_debug ("Path too long");
		response->status = FH_STATUS_REQUEST_URI_TOO_LONG;
		return true;
	}

	normalized_path_len = (size_t) path_buf_len;

	if (!path_normalize (normalized_path, path_buf, &normalized_path_len))
	{
		fh_pr_debug ("Path normalization failed");
		response->status = FH_STATUS_BAD_REQUEST;
		return true;
	}

	/* At this point, we have successfully normalized the file path under the
	 * docroot. */
	fh_pr_debug ("Path: %s", normalized_path);

	fd_t fd = open (normalized_path, O_RDONLY);

	if (fd < 0)
	{
		response->status = errno == ENOENT ? FH_STATUS_NOT_FOUND
						   : (errno == EACCES || errno == EPERM)
							   ? FH_STATUS_FORBIDDEN
							   : FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	struct stat64 st;

	if (fstat64 (fd, &st) < 0)
	{
		response->status = errno == ENOENT ? FH_STATUS_NOT_FOUND
						   : (errno == EACCES || errno == EPERM)
							   ? FH_STATUS_FORBIDDEN
							   : FH_STATUS_INTERNAL_SERVER_ERROR;
		close (fd);
		return true;
	}

	fh_router_handle_static_file (router, conn, request, response, fd, &st);
	return true;
}