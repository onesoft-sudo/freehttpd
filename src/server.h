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

#define MAX_REQUEST_THREADS 4

enum fhttpd_config
{
    FHTTPD_CONFIG_PORTS,
    FHTTPD_CONFIG_WORKER_COUNT,
    FHTTPD_CONFIG_DOCROOT,
    FHTTPD_CONFIG_MAX_CONNECTIONS,
    FHTTPD_CONFIG_CLIENT_HEADER_TIMEOUT,
    FHTTPD_CONFIG_CLIENT_BODY_TIMEOUT,
    FHTTPD_CONFIG_RECV_TIMEOUT,
    FHTTPD_CONFIG_SEND_TIMEOUT,
    FHTTPD_CONFIG_LOG_LEVEL,
    FHTTPD_CONFIG_MAX_BODY_SIZE,
    FHTTPD_CONFIG_MAX
};

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

    void *config[FHTTPD_CONFIG_MAX];

    /* (fd_t) => (struct fhttpd_addrinfo *) */
    struct itable *sockaddr_in_table;

    /* (fd_t) => (struct fhttpd_connection *) */
    struct itable *connections;
    uint64_t last_connection_id;
};

struct fhttpd_master
{
    pid_t pid;
    pid_t *workers;
    size_t worker_count;
    void *config[FHTTPD_CONFIG_MAX];
};

struct fhttpd_master *fhttpd_master_create (void);
bool fhttpd_master_start (struct fhttpd_master *master);
void fhttpd_master_destroy (struct fhttpd_master *master);

void *fhttpd_get_config (struct fhttpd_master *master,
                                enum fhttpd_config config);
void fhttpd_set_config (struct fhttpd_master *master,
                               enum fhttpd_config config, void *value);

#endif /* FHTTPD_SERVER_H */
