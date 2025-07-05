#include <unistd.h>

#include "send.h"

bool 
event_send (struct fh_server *server, const xevent_t *event)
{
	struct fh_conn *conn = itable_get (server->connections, event->data.fd);

	if (!conn)
	{
		fh_pr_err ("Socket %d does not have an associated connection object", event->data.fd);
		xpoll_del (server->xpoll_fd, event->data.fd, XPOLLIN);
		close (event->data.fd);
		return false;
	}

    send (conn->client_sockfd, "HTTP/1.1 200 OK\r\nServer: freehttpd\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", 76, 0);
    fh_server_close_conn (server, conn);
    return true;
}