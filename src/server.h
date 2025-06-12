#ifndef FHTTPD_SERVER_H
#define FHTTPD_SERVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>

#include "error.h"
#include "protocol.h"

enum fhttpd_config
{
    FHTTPD_CONFIG_PORTS,
    FHTTPD_CONFIG_DOCROOT,
    FHTTPD_CONFIG_MAX_CONNECTIONS,
    FHTTPD_CONFIG_CLIENT_RECV_TIMEOUT,
    FHTTPD_CONFIG_CLIENT_HEADER_TIMEOUT,
    FHTTPD_CONFIG_CLIENT_BODY_TIMEOUT,
    FHTTPD_CONFIG_LOG_LEVEL,
    FHTTPD_CONFIG_MAX
};

struct fhttpd_connection
{
    uint64_t id;
    protocol_t protocol;

    int client_sockfd;
    struct sockaddr_in client_addr;
    socklen_t addr_len;

    char buffer[H2_PREFACE_SIZE];
    size_t buffer_len;

    struct fhttpd_request *requests;
    size_t num_requests;

    struct http11_parser_ctx *http11_parser_ctx;

    uint64_t last_recv_activity_ts;
    uint64_t created_at_ts;
};

struct fhttpd_server;

struct fhttpd_server *fhttpd_server_create (void);
void fhttpd_server_destroy (struct fhttpd_server *server);
int fhttpd_server_run (struct fhttpd_server *server);
void *fhttpd_server_get_config (struct fhttpd_server *server,
                                enum fhttpd_config config);
void fhttpd_server_set_config (struct fhttpd_server *server,
                               enum fhttpd_config config, void *value);
pid_t fhttpd_server_get_master_pid (struct fhttpd_server *server);

ssize_t fhttpd_connection_recv (struct fhttpd_connection *connection, void *buf,
                                size_t len, int flags);

#endif /* FHTTPD_SERVER_H */