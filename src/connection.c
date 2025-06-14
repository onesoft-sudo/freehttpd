#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

#include "connection.h"
#include "utils.h"

struct fhttpd_connection *
fhttpd_connection_create (uint64_t id, fd_t client_sockfd)
{
    struct fhttpd_connection *conn = calloc (1, sizeof (struct fhttpd_connection));

    if (!conn)
        return NULL;

    conn->id = id;
    conn->client_sockfd = client_sockfd;
    conn->last_recv_timestamp = utils_get_current_timestamp ();
    conn->created_at = conn->last_recv_timestamp;

    return conn;
}

void
fhttpd_connection_free (struct fhttpd_connection *conn)
{
    if (!conn)
        return;

    free (conn);
}

ssize_t
fhttpd_connection_recv (struct fhttpd_connection *conn, void *buf, size_t size, int flags)
{
    ssize_t bytes_read = recv (conn->client_sockfd, buf, size, flags);

    if (bytes_read < 0)
        return bytes_read;

    int err = errno;

    conn->last_recv_timestamp = utils_get_current_timestamp ();
    errno = err;

    return bytes_read;
}
