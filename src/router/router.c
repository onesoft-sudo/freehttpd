#define _GNU_SOURCE

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "router"

#include "core/conf.h"
#include "core/conn.h"
#include "http/http1_request.h"
#include "http/http1_response.h"
#include "log/log.h"
#include "router.h"
#include "utils/utils.h"

bool
fh_router_init (struct fh_router *router, struct fh_server *server)
{
	router->default_route = malloc (sizeof (struct fh_route));

	if (!router->default_route)
		return false;

	router->routes = strtable_create (0);

	if (!router->routes)
		return false;

	router->default_route->handler = FH_HANDLER_FILESYSTEM;
	router->default_route->flags = FH_HANDLER_FILESYSTEM_FLAGS;
	router->default_route->path = NULL;
	router->server = server;
	return true;
}

void
fh_router_free (struct fh_router *router)
{
	free (router->default_route);
	strtable_destroy (router->routes);
}

bool
fh_router_handle (struct fh_router *router, struct fh_conn *conn,
				  const struct fh_request *request)
{
	struct fh_route *route = NULL;
	struct fh_http1_res_ctx *ctx = conn->res_ctx;
	pool_t *child_pool = NULL;

	if (!ctx)
	{
		child_pool = fh_pool_create (0);

		if (!child_pool)
		{
			fh_server_close_conn (router->server, conn);
			return true;
		}

		ctx = fh_http1_res_ctx_create (child_pool);

		if (!ctx)
		{
			fh_server_close_conn (router->server, conn);
			return true;
		}
	}

	bool initial_call = !conn->res_ctx;

	if (!conn->res_ctx)
		conn->res_ctx = ctx;

	if (!route)
		route = router->default_route;

	if ((initial_call || route->flags & ~FH_ROUTE_CALL_ONCE))
	{
		if (!route->handler (router, conn, request, ctx->response))
		{
			fh_server_close_conn (router->server, conn);
			return true;
		}
	}

	if (!fh_http1_send_response (ctx, conn))
	{
		fh_pr_err ("Failed to send response");
		fh_server_close_conn (router->server, conn);
		return true;
	}

	if (ctx->state == FH_RES_STATE_DONE)
	{
		fh_pr_debug ("Response sent successfully");
		fh_server_close_conn (router->server, conn);
		return true;
	}

	if (would_block ())
	{
		fh_pr_debug ("Need to wait to send further data");
		return true;
	}

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
		response->status = errno == ENOENT ? FH_STATUS_NOT_FOUND : (errno == EACCES || errno == EPERM) ? FH_STATUS_FORBIDDEN : FH_STATUS_INTERNAL_SERVER_ERROR;
		return true;
	}

	struct stat64 st;

	if (fstat64 (fd, &st) < 0)
	{
		response->status = errno == ENOENT ? FH_STATUS_NOT_FOUND : (errno == EACCES || errno == EPERM) ? FH_STATUS_FORBIDDEN : FH_STATUS_INTERNAL_SERVER_ERROR;
		close (fd);
		return true;
	}

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
	buf->file_fd = fd;
	buf->file_off = 0;
	buf->file_len = st.st_size;

	response->body_start->next = NULL;
	response->body_start->is_eos = true;
	response->content_length = st.st_size;
	response->use_default_error_response = false;
	response->status = FH_STATUS_OK;

	fh_pr_debug ("Successfully generated response");
	return true;
}
