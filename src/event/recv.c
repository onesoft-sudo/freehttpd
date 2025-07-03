#define FH_LOG_MODULE_NAME "event/recv"

#include <stdlib.h>
#include <unistd.h>

#include "recv.h"
#include "compat.h"
#include "core/conn.h"
#include "core/server.h"
#include "log/log.h"

bool
event_recv (struct fh_server *server, const xevent_t *event)
{
	struct fh_conn *conn = itable_get (server->connections, event->data.fd);

	if (!conn)
	{
		fh_pr_err ("Socket %d does not have an associated connection object", event->data.fd);
		xpoll_del (server->xpoll_fd, event->data.fd, XPOLLIN);
		close (event->data.fd);
		return false;
	}

	fh_pr_info ("connection %lu: recv called", conn->id);
    fh_server_close_conn (server, conn);
	return true;
}
