#ifndef FHTTPD_PROTOCOL_H
#define FHTTPD_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define H2_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define H2_PREFACE_SIZE (sizeof (H2_PREFACE) - 1)

enum fhttpd_protocol
{
    FHTTPD_PROTOCOL_UNKNOWN,
    FHTTPD_PROTOCOL_HTTP1x,
    FHTTPD_PROTOCOL_H2
};

typedef enum fhttpd_protocol protocol_t;

enum fhttpd_method
{
    FHTTPD_METHOD_GET,
    FHTTPD_METHOD_POST,
    FHTTPD_METHOD_PUT,
    FHTTPD_METHOD_DELETE,
    FHTTPD_METHOD_HEAD,
    FHTTPD_METHOD_OPTIONS,
    FHTTPD_METHOD_PATCH,
    FHTTPD_METHOD_CONNECT,
    FHTTPD_METHOD_TRACE
};

struct fhttpd_header
{
    char *name;
    char *value;
    size_t name_length;
    size_t value_length;
};

struct fhttpd_headers
{
    struct fhttpd_header *headers;
    size_t count;
};

struct fhttpd_request
{
    protocol_t protocol;
};

const char *fhttpd_protocol_to_string (enum fhttpd_protocol protocol);
enum fhttpd_protocol fhttpd_string_to_protocol (const char *protocol_str);

int fhttpd_stream_detect_protocol (int sockfd, size_t *bytes_peeked);

#endif /* FHTTPD_PROTOCOL_H */