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

#ifndef FH_ROUTER_H
#define FH_ROUTER_H

#include <stdbool.h>
#include "hash/strtable.h"
#include "http/protocol.h"
#include "core/conn.h"
#include "core/server.h"
#include "filesystem.h"

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
	struct strtable *static_routes;
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
