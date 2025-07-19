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

#ifndef FH_ROUTER_FILESYSTEM_H
#define FH_ROUTER_FILESYSTEM_H

#include <stdbool.h>

#include "core/stream.h"
#include "core/conn.h"
#include "http/protocol.h"
#include "http/http1_request.h"
#include "http/http1_response.h"

bool fh_router_handle_filesystem (struct fh_router *router, struct fh_conn *conn,
							 const struct fh_request *request,
							 struct fh_response *response);

#endif /* FH_ROUTER_FILESYSTEM_H */
