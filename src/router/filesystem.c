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

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "router/fs"

#include "core/conn.h"
#include "core/stream.h"
#include "filesystem.h"
#include "http/http1_request.h"
#include "http/http1_response.h"
#include "modules/mod_autoindex.h"
#include "router.h"
#include "utils/utils.h"

static inline bool
fh_router_handle_directory_index (struct fh_router *router,
								  struct fh_conn *conn,
								  const struct fh_request *request,
								  struct fh_response *response,
								  const char *filename, size_t filename_len,
								  const struct stat64 *st)
{
	struct fh_autoindex autoindex = {
		.conn = conn,
		.filename = filename,
		.filename_len = filename_len,
		.request = request,
		.response = response,
		.router = router,
		.st = st,
	};

	return fh_autoindex_handle (&autoindex);
}

static bool
fh_router_handle_static_file (struct fh_router *router, struct fh_conn *conn,
							  const struct fh_request *request,
							  struct fh_response *response,
							  const char *filename, size_t filename_len,
							  const struct stat64 *st)
{
	(void) router;
	(void) request;
	(void) conn;
	(void) filename_len;

	response->content_length = st->st_size;
	response->status = FH_STATUS_OK;

	if (request->method == FH_METHOD_HEAD)
	{
		response->body_start = NULL;
		response->use_default_error_response = false;
		return true;
	}

	response->body_start = fh_pool_alloc (
		response->pool, sizeof (struct fh_link) + sizeof (struct fh_buf));

	if (unlikely (!response->body_start))
	{
		response->status = FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	response->body_start->buf = (struct fh_buf *) (response->body_start + 1);
	struct fh_buf *buf = response->body_start->buf;

	fd_t fd = open (filename, O_RDONLY);

	if (fd < 0)
	{
		response->status = errno == ENOENT ? FH_STATUS_NOT_FOUND
						   : (errno == EACCES || errno == EPERM)
							   ? FH_STATUS_FORBIDDEN
							   : FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

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

	response->use_default_error_response = true;

	if (request->method != FH_METHOD_GET && request->method != FH_METHOD_HEAD)
	{
		fh_pr_debug ("Not GET or HEAD");
		response->status = FH_STATUS_METHOD_NOT_ALLOWED;
		return true;
	}

	if (request->method == FH_METHOD_HEAD)
		response->no_send_body = true;

	int path_buf_len = 0;
	size_t normalized_path_len = 0;
	char path_buf[PATH_MAX + 1] = { 0 };
	char normalized_path[PATH_MAX + 1] = { 0 };

	if (request->uri_len >= INT32_MAX
		|| (path_buf_len = snprintf (path_buf, sizeof path_buf, "%s/%.*s",
									 conn->config->docroot,
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
	struct stat64 st;

	if (stat64 (normalized_path, &st) < 0)
	{
		response->status = errno == ENOENT ? FH_STATUS_NOT_FOUND
						   : (errno == EACCES || errno == EPERM)
							   ? FH_STATUS_FORBIDDEN
							   : FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	if (S_ISDIR (st.st_mode))
		return fh_router_handle_directory_index (router, conn, request,
												 response, normalized_path,
												 normalized_path_len, &st);
	else
		return fh_router_handle_static_file (router, conn, request, response,
											 normalized_path,
											 normalized_path_len, &st);

	return true;
}
