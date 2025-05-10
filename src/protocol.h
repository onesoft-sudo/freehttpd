#ifndef FREEHTTPD_PROTOCOL_H
#define FREEHTTPD_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#include "buffer.h"
#include "lstring.h"

#define HTTP_HEADER_MAX_COUNT          128
#define HTTP_HEADER_MAX_NAME_LENGTH    256
#define HTTP_HEADER_MAX_VALUE_LENGTH   8192
#define HTTP_METHOD_MAX_LENGTH         16
#define HTTP_URI_MAX_LENGTH            2048
#define HTTP_VERSION_MAX_LENGTH        8

#define HTTP_CONTENT_LENGTH_MAX       (1024 * 1024 * 1024)

enum http_response_code
{
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_NOT_ACCEPTABLE = 406,
    HTTP_NOT_FOUND = 404,
    HTTP_INTERNAL_SERVER_ERROR = 500
};

enum http_version
{
    HTTP_VERSION_09,
    HTTP_VERSION_10,
    HTTP_VERSION_11,
    HTTP_VERSION_20,
    HTTP_VERSION_UNKNOWN
};

struct http_header
{
    lstring_t name;
    lstring_t value;
};

struct http_request
{
    lstring_t method;
    lstring_t uri;
    lstring_t query;
    enum http_version version;
    
    struct http_header *headers;
    size_t header_count;

    struct buffer *buffered_body;
};

struct http_response
{
    enum http_version version;
    enum http_response_code code;
    struct http_header *headers;
    size_t header_count;
    struct buffer *body;
};

void http_request_destroy(struct http_request *request);
struct http_request *http_request_create();
void http_request_destroy_inner(struct http_request *request);

void http_response_destroy_inner(struct http_response *response);
void http_response_destroy(struct http_response *response);
struct http_response *http_response_create();

bool http_response_add_header(struct http_response *response, const char *name, const char *value);

const char *http_status_code_to_text(enum http_response_code code);
const char *http_version_string(enum http_version version);

bool is_method_with_body(const char *method);

#endif /* FREEHTTPD_PROTOCOL_H */