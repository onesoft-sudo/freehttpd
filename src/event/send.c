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

#include <unistd.h>

#define FH_LOG_MODULE_NAME "event/send"

#include "core/conn.h"
#include "http/protocol.h"
#include "router/router.h"
#include "log/log.h"
#include "send.h"

bool
event_send (struct fh_server *server, const xevent_t *event)
{
	struct fh_conn *conn = itable_get (server->connections, event->data.fd);

	if (!conn)
	{
		fh_pr_err ("Socket %d does not have an associated connection object", event->data.fd);
		xpoll_del (server->xpoll_fd, event->data.fd, XPOLLOUT);
		close (event->data.fd);
		return false;
	}

	if (!fh_router_handle (server->router, conn, conn->requests->tail))
	{
		fh_pr_err ("Connection #%lu: failed to route", conn->id);
		fh_server_close_conn (server, conn);
		return true;
	}

	return true;
}
