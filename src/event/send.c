#define _GNU_SOURCE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "send.h"

bool
fh_event_send (struct fhttpd_server *server, struct fh_conn *conn)
{
    send (conn->client_sockfd, "HTTP/1.1 200 OK\r\nServer: freehttpd\r\nConnection: close\r\nContent-Length: 0\r\n\r\n", 76, 0);
    fhttpd_server_conn_close (server, conn);
    return true;
}