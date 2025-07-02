#ifndef FH_CORE_SERVER_H
#define FH_CORE_SERVER_H

#include <stdbool.h>
#include <stddef.h>

#include "types.h"
#include "conf.h"

#define FH_SERVER_MAX_SOCKETS 128

struct fh_server
{
    fd_t sockets[FH_SERVER_MAX_SOCKETS];
    size_t socket_count;
    struct fhttpd_config *config;
    bool should_exit : 1;
};

struct fh_server *fh_server_create (struct fhttpd_config *config);
void fh_server_destroy (struct fh_server *server);
void fh_server_loop (struct fh_server *server);
bool fh_server_listen (struct fh_server *server);

#endif /* FH_CORE_SERVER_H */