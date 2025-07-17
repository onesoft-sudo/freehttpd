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
	struct fh_http1_res_ctx *ctx = conn->io_ctx.h1.res_ctx;
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

	bool initial_call = !conn->io_ctx.h1.res_ctx;

	if (!conn->io_ctx.h1.res_ctx)
		conn->io_ctx.h1.res_ctx = ctx;

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
