#ifndef FHTTPD_CONNECTION_H
#define FHTTPD_CONNECTION_H

#include <arpa/inet.h>
#include <stdint.h>

#include "http1.h"
#include "protocol.h"
#include "types.h"

struct fhttpd_connection
{
    uint64_t id;

    fd_t client_sockfd;
    protocol_t protocol;
    char exact_protocol[4];

    uint64_t last_recv_timestamp;
    uint64_t last_send_timestamp;
    uint64_t created_at;

    union
    {
        char protobuf[H2_PREFACE_SIZE];
    } buffers;

    size_t buffer_size;

    union
    {
        struct
        {
            struct http1_parser_ctx http1_req_ctx;
            struct http1_response_ctx http1_res_ctx;
        };
    };

    struct fhttpd_request *requests;
    size_t request_count;

    struct fhttpd_response *responses;
    size_t response_count;

    uint16_t port;
    char host[INET_ADDRSTRLEN];
};

struct fhttpd_connection *fhttpd_connection_create (uint64_t id, fd_t client_sockfd);
void fhttpd_connection_close (struct fhttpd_connection *conn);
ssize_t fhttpd_connection_recv (struct fhttpd_connection *conn, void *buf, size_t size, int flags);
bool fhttpd_connection_detect_protocol (struct fhttpd_connection *conn);
ssize_t fhttpd_connection_send (struct fhttpd_connection *conn, const void *buf, size_t size, int flags);
ssize_t fhttpd_connection_sendfile (struct fhttpd_connection *conn, int src_fd, off_t *offset, size_t count);
bool fhttpd_connection_defer_response (struct fhttpd_connection *conn, size_t response_index,
                                       const struct fhttpd_response *response);
bool fhttpd_connection_defer_error_response (struct fhttpd_connection *conn, size_t response_index,
                                             enum fhttpd_status code);
bool fhttpd_connection_send_response (struct fhttpd_connection *conn, size_t response_index,
                                      const struct fhttpd_response *response);

#endif /* FHTTPD_CONNECTION_H */
