#ifndef FH_ROUTER_H
#define FH_ROUTER_H

#include <stdbool.h>
#include "hash/strtable.h"
#include "http/protocol.h"
#include "core/conn.h"
#include "core/server.h"

struct fh_router;

typedef bool (*fh_route_handler_t) (struct fh_router *router, struct fh_conn *conn, const struct fh_request *request, struct fh_response *response);

struct fh_route
{
	const char *path;
	fh_route_handler_t handler;
	uint32_t flags;
};

struct fh_router
{
	struct fh_server *server;
	/* (const char *) => (struct fh_route *) */
	struct strtable *routes;
	struct fh_route *default_route;
};

bool fh_router_init (struct fh_router *router, struct fh_server *server);
void fh_router_free (struct fh_router *router);
bool fh_router_handle (struct fh_router *router, struct fh_conn *conn, const struct fh_request *request);

bool fh_router_handle_filesystem (struct fh_router *router, struct fh_conn *conn, const struct fh_request *request, struct fh_response *response);

#define FH_ROUTE_CALL_ONCE 0x1

#define FH_HANDLER_FILESYSTEM (&fh_router_handle_filesystem)

#define FH_HANDLER_FILESYSTEM_FLAGS FH_ROUTE_CALL_ONCE

#endif /* FH_ROUTER_H */
