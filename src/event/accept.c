#define _GNU_SOURCE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>

#include "accept.h"
#include "utils/utils.h"

bool
fh_event_accept (struct fhttpd_server *server, const xevent_t *event)
{
	fd_t sockfd = event->data.fd;
	fd_t client_sockfd;
	struct sockaddr_in client_addr = {0};
	socklen_t client_addr_len = sizeof client_addr;

#ifdef __linux__
	client_sockfd = accept4 (sockfd, (struct sockaddr *) &client_addr, &client_addr_len, SOCK_NONBLOCK);

	if (client_sockfd < 0)
		return false;
#else /* not __linux__ */
	client_sockfd = accept (sockfd, (struct sockaddr *) &client_addr, &client_addr_len);

	if (client_sockfd < 0)
		return false;

	if (!fd_set_nonblocking (client_sockfd))
	{
		close (client_sockfd);
		return false;
	}
#endif /* __linux__ */

	if (!xpoll_add (server->xpoll, client_sockfd, XPOLLIN | XPOLLET, 0))
	{
		close (client_sockfd);
		return false;
	}

	char client_ip[INET_ADDRSTRLEN] = {0};

	inet_ntop (AF_INET, &client_addr.sin_addr, client_ip, sizeof (client_ip));
	fhttpd_log_info ("Accepted connection from: %s:%d", client_ip, ntohs (client_addr.sin_port));

	return true;
}
