#ifndef FH_CORE_SERVER_H
#define FH_CORE_SERVER_H

#include <stdbool.h>
#include <stddef.h>

#include "types.h"
#include "conf.h"
#include "event/xpoll.h"
#include "hash/itable.h"

#define FH_SERVER_MAX_SOCKETS 128

struct fh_server
{
    struct fhttpd_config *config;
    fd_t xpoll_fd;
    bool should_exit : 1;
    
    /* (fd_t) => (struct sockaddr_in *) */
    struct itable *sockfd_table;
};

struct fh_server *fh_server_create (struct fhttpd_config *config);
void fh_server_destroy (struct fh_server *server);
void fh_server_loop (struct fh_server *server);
bool fh_server_listen (struct fh_server *server);

#endif /* FH_CORE_SERVER_H */