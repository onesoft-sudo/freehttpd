#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "protocol.h"

#define STREAM_DETECT_INITIAL_READ_SIZE 64

static_assert (sizeof (H2_PREFACE) - 1 <= STREAM_DETECT_INITIAL_READ_SIZE,
               "HTTP2_PREFACE size exceeds initial read buffer size");

const char *
fhttpd_protocol_to_string (enum fhttpd_protocol protocol)
{
    switch (protocol)
        {
        case FHTTPD_PROTOCOL_HTTP_1_x:
            return "HTTP/1.x";
        case FHTTPD_PROTOCOL_H2:
            return "h2";
        default:
            return "Unknown Protocol";
        }
}

enum fhttpd_protocol
fhttpd_string_to_protocol (const char *protocol_str)
{
    if (strcmp (protocol_str, "HTTP/1.0") == 0)
        return FHTTPD_PROTOCOL_HTTP_1_x;
    else if (strcmp (protocol_str, "HTTP/1.1") == 0)
        return FHTTPD_PROTOCOL_HTTP_1_x;
    else if (strcmp (protocol_str, "HTTP/2.0") == 0
             || strcmp (protocol_str, "h2") == 0
             || strcmp (protocol_str, "h2c") == 0)
        return FHTTPD_PROTOCOL_H2;
    else
        return -1;
}

enum fhttpd_protocol
fhttpd_stream_detect_protocol (int sockfd)
{
    uint8_t local_read_buf[STREAM_DETECT_INITIAL_READ_SIZE];
    ssize_t size = sizeof (H2_PREFACE) - 1;
    ssize_t bytes_read = recv (sockfd, local_read_buf, size, MSG_PEEK);

    if (bytes_read <= 0)
        return -errno;

    if (bytes_read == size && memcmp (local_read_buf, H2_PREFACE, size) == 0)
        return FHTTPD_PROTOCOL_H2;

    return FHTTPD_PROTOCOL_HTTP_1_x;
}