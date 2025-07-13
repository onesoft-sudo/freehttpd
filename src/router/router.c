#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define FH_LOG_MODULE_NAME "router"

#include "core/conf.h"
#include "core/conn.h"
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
fh_router_handle (struct fh_router *router, struct fh_conn *conn, const struct fh_request *request)
{
	const char *path = request->uri;
	struct fh_route *route = strtable_get (router->routes, path);
	struct fh_response response = { 0 };

	if (!route)
		route = router->default_route;

	if (!route->handler (router, conn, request, &response))
	{
		fh_server_close_conn (router->server, conn);
		return true;
	}

	if (response.use_default_error_response)
	{
		fh_conn_send_err_response (conn, response.status);
		fh_server_close_conn (router->server, conn);
		return true;
	}

	/* TODO */
	dprintf (conn->client_sockfd, "HTTP/1.1 %d %s\r\nServer: freehttpd\r\nContent-Length: 13\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\nHello world!\n",
			 response.status, fh_get_status_text (response.status, NULL));
	fh_server_close_conn (router->server, conn);
	return true;
}

bool
fh_router_handle_filesystem (struct fh_router *router, struct fh_conn *conn, const struct fh_request *request,
							 struct fh_response *response)
{
	(void) router;

	char path_buf[PATH_MAX + 1] = { 0 };
	char normalized_path[PATH_MAX + 1] = { 0 };
	int path_buf_len = 0;
	size_t normalized_path_len = 0;

	if (request->uri_len >= INT32_MAX
		|| (path_buf_len = snprintf (path_buf, sizeof path_buf, "%s/%.*s", conn->config->host_config->docroot, (int) request->uri_len,
					 request->uri))
			   >= PATH_MAX || path_buf_len < 0) 
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

	fh_pr_debug ("Path: %s", normalized_path);
	response->status = FH_STATUS_OK;
	return true;
}
