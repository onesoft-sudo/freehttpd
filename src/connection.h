#ifndef FHTTPD_CONNECTION_H
#define FHTTPD_CONNECTION_H

#include <stdint.h>

#include "types.h"
#include "protocol.h"

struct fhttpd_connection
{
    uint64_t id;

    fd_t client_sockfd;
    protocol_t protocol;

    uint64_t last_recv_timestamp;
    uint64_t created_at;

//    union
//    {
//        struct http11_parser_ctx http11_ctx;
//    };
};


struct fhttpd_connection *fhttpd_connection_create (uint64_t id, fd_t client_sockfd);
void fhttpd_connection_free (struct fhttpd_connection *conn);
ssize_t fhttpd_connection_recv (struct fhttpd_connection *conn, void *buf, size_t size, int flags);

#endif /* FHTTPD_CONNECTION_H */
