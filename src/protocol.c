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
        case FHTTPD_PROTOCOL_HTTP1x:
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
    if (strcmp (protocol_str, "HTTP/1.0") == 0
        || strcmp (protocol_str, "HTTP/1.1") == 0)
        return FHTTPD_PROTOCOL_HTTP1x;
    else if (strcmp (protocol_str, "HTTP/2.0") == 0
             || strcmp (protocol_str, "h2") == 0
             || strcmp (protocol_str, "h2c") == 0)
        return FHTTPD_PROTOCOL_H2;
    else
        return -1;
}

const char *
fhttpd_method_to_string (enum fhttpd_method method)
{
    switch (method)
    {
        case FHTTPD_METHOD_GET:
            return "GET";
        case FHTTPD_METHOD_POST:
            return "POST";
        case FHTTPD_METHOD_PUT:
            return "PUT";
        case FHTTPD_METHOD_DELETE:
            return "DELETE";
        case FHTTPD_METHOD_HEAD:
            return "HEAD";
        case FHTTPD_METHOD_OPTIONS:
            return "OPTIONS";
        case FHTTPD_METHOD_PATCH:
            return "PATCH";
        case FHTTPD_METHOD_CONNECT:
            return "CONNECT";
        case FHTTPD_METHOD_TRACE:
            return "TRACE";
        default:
            return "Unknown Method";
    }
}