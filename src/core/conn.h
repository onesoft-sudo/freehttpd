#ifndef FH_CORE_CONN_H
#define FH_CORE_CONN_H

#include <stdint.h>
#include <sys/types.h>
#include <stddef.h>
#include <netinet/in.h>
#include "types.h"
#include "mm/pool.h"
#include "stream.h"

struct fh_conn
{
    object_id_t id;
    fd_t client_sockfd;
    struct sockaddr_in *client_addr;
    const struct sockaddr_in *server_addr;
    pool_t *pool;
    struct fh_stream *stream;

    union {
        struct fh_http1_ctx *req_ctx;
    };
};

struct fh_conn *fh_conn_create (fd_t client_sockfd, const struct sockaddr_in *client_addr, const struct sockaddr_in *server_addr);
void fh_conn_destroy (struct fh_conn *conn);

#endif /* FH_CORE_CONN_H */