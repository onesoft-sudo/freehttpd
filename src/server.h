#ifndef FHTTPD_SERVER_H
#define FHTTPD_SERVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>

#include "error.h"
#include "protocol.h"
#include "types.h"
#include "itable.h"
#include "strtable.h"
#include "conf.h"
#include "master.h"

#define MAX_REQUEST_THREADS 4

struct fhttpd_addrinfo
{
    struct sockaddr_in addr;
    socklen_t addr_len;
    char host[INET_ADDRSTRLEN];
    uint16_t port;
    fd_t sockfd;
};

struct fhttpd_server
{
    pid_t master_pid;
    pid_t pid;

    fd_t epoll_fd;
    fd_t timer_fd;

    fd_t *listen_fds;
    size_t listen_fd_count;

    struct fhttpd_config *config;

    /* (const char *) => (struct fhttpd_config_host *) */
    struct strtable *host_config_table;

    /* (fd_t) => (struct fhttpd_addrinfo *) */
    struct itable *sockaddr_in_table;

    /* (fd_t) => (struct fhttpd_connection *) */
    struct itable *connections;
    uint64_t last_connection_id;
};

_Noreturn void fhttpd_server_loop (struct fhttpd_server *server);

struct fhttpd_server *fhttpd_server_create (const struct fhttpd_master *master, struct fhttpd_config *config);
bool fhttpd_server_prepare (struct fhttpd_server *server);
void fhttpd_server_destroy (struct fhttpd_server *server);

#endif /* FHTTPD_SERVER_H */
