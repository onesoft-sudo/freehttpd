#ifndef FHTTPD_PROTOCOL_H
#define FHTTPD_PROTOCOL_H

#include <stdbool.h>
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

enum fhttpd_status
{
    FHTTPD_STATUS_OK = 200,
    FHTTPD_STATUS_CREATED = 201,
    FHTTPD_STATUS_ACCEPTED = 202,
    FHTTPD_STATUS_NO_CONTENT = 204,
    FHTTPD_STATUS_BAD_REQUEST = 400,
    FHTTPD_STATUS_UNAUTHORIZED = 401,
    FHTTPD_STATUS_FORBIDDEN = 403,
    FHTTPD_STATUS_NOT_FOUND = 404,
    FHTTPD_STATUS_REQUEST_URI_TOO_LONG = 414,
    FHTTPD_STATUS_INTERNAL_SERVER_ERROR = 500,
    FHTTPD_STATUS_NOT_IMPLEMENTED = 501,
    FHTTPD_STATUS_SERVICE_UNAVAILABLE = 503,
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
    struct fhttpd_header *list;
    size_t count;
};

struct fhttpd_request
{
    protocol_t protocol;
    enum fhttpd_method method;
    char *uri;
    size_t uri_len;
    struct fhttpd_headers headers;
    uint8_t *body;
    uint64_t body_len;
};

struct fhttpd_response
{
    enum fhttpd_status status;
    struct fhttpd_headers headers;
    uint8_t *body;
    uint64_t body_len;

    bool set_content_length;
    bool is_deferred;
    bool use_builtin_error_response;
    bool sent;
};

const char *fhttpd_protocol_to_string (enum fhttpd_protocol protocol);
enum fhttpd_protocol fhttpd_string_to_protocol (const char *protocol_str);

const char *fhttpd_method_to_string (enum fhttpd_method method);

bool fhttpd_validate_header_name (const char *name, size_t len);

const char *fhttpd_get_status_text (enum fhttpd_status code);
const char *fhttpd_get_status_description (enum fhttpd_status code);

bool fhttpd_header_add (struct fhttpd_headers *headers, const char *name, const char *value, size_t name_length, size_t value_length);
bool fhttpd_header_add_noalloc (struct fhttpd_headers *headers, size_t index, const char *name, const char *value, size_t name_length, size_t value_length);

#endif /* FHTTPD_PROTOCOL_H */
