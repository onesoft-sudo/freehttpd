#ifndef FHTTPD_CONNECTION_H
#define FHTTPD_CONNECTION_H

#include <stdint.h>

#include "types.h"
#include "protocol.h"
#include "http1.h"

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
       struct http1_parser_ctx http1_ctx;
   };

   struct fhttpd_request *requests;
   size_t request_count;
};


struct fhttpd_connection *fhttpd_connection_create (uint64_t id, fd_t client_sockfd);
void fhttpd_connection_free (struct fhttpd_connection *conn);
ssize_t fhttpd_connection_recv (struct fhttpd_connection *conn, void *buf, size_t size, int flags);
bool fhttpd_connection_detect_protocol (struct fhttpd_connection *conn);
bool fhttpd_connection_send (struct fhttpd_connection *conn, const void *buf, size_t size, int flags);
bool fhttpd_connection_sendfile (struct fhttpd_connection *conn, int src_fd, off_t *offset, size_t count);
bool fhttpd_connection_error_response (struct fhttpd_connection *conn, enum fhttpd_status code);

#endif /* FHTTPD_CONNECTION_H */
