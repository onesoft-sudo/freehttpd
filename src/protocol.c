#include <assert.h>
#include <string.h>

#include "buffer.h"
#include "protocol.h"

static const char *HTTP_METHODS_WITHOUT_BODY[] = {
    "GET",
    "HEAD",
    "OPTIONS",
    "TRACE",
    NULL
};

bool is_method_with_body(const char *method)
{
    for (size_t i = 0; HTTP_METHODS_WITHOUT_BODY[i] != NULL; i++)
    {
        if (strcmp(method, HTTP_METHODS_WITHOUT_BODY[i]) == 0)
            return false;
    }

    return true;
}

struct http_request *http_request_create()
{
    struct http_request *request = calloc(1, sizeof(struct http_request));

    if (!request)
        return NULL;

    return request;
}

struct http_response *http_response_create()
{
    struct http_response *response = calloc(1, sizeof(struct http_response));

    if (!response)
        return NULL;

    return response;
}

void http_response_destroy_inner(struct http_response *response)
{
    if (!response)
        return;

    for (size_t i = 0; i < response->header_count; i++)
    {
        lstring_destroy(response->headers[i].name);
        lstring_destroy(response->headers[i].value);
    }

    free(response->headers);
    buffer_destroy(response->body);
}

void http_response_destroy(struct http_response *response)
{
    http_response_destroy_inner(response);
    free(response);
}

void http_request_destroy_inner(struct http_request *request)
{
    if (!request)
        return;

    lstring_destroy(request->method);
    lstring_destroy(request->uri);

    if (request->query)
        lstring_destroy(request->query);

    for (size_t i = 0; i < request->header_count; i++)
    {
        lstring_destroy(request->headers[i].name);
        lstring_destroy(request->headers[i].value);
    }

    free(request->headers);
    buffer_destroy(request->buffered_body);
}

void http_request_destroy(struct http_request *request)
{
    http_request_destroy_inner(request);
    free(request);
}

bool http_response_add_header(struct http_response *response, const char *name, const char *value)
{
    response->headers = realloc(response->headers, (++response->header_count) * sizeof(struct http_header));

    if (!response->headers)
        return false;

    lstring_t name_l = lstring_create(name, 0);

    if (!name_l)
        return false;

    lstring_t value_l = lstring_create(value, 0);

    if (!value_l)
    {
        lstring_destroy(name_l);
        return false;
    }

    response->headers[response->header_count - 1].name = name_l;
    response->headers[response->header_count - 1].value = value_l;
    return true;
}

const char *http_status_code_to_text(enum http_response_code code)
{
    switch (code) {
        case HTTP_OK:                    return "OK";
        case HTTP_BAD_REQUEST:           return "Bad Request";
        case HTTP_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HTTP_NOT_FOUND:             return "Not Found";
        case HTTP_METHOD_NOT_ALLOWED:    return "Method Not Allowed";
        case HTTP_NOT_ACCEPTABLE:        return "Not Acceptable";

        default:
            assert(false && "Invalid status code");
            return NULL;
    }
}

const char *http_version_string(enum http_version version)
{
    switch (version) {
        case HTTP_VERSION_09:  return "HTTP/0.9";
        case HTTP_VERSION_10:  return "HTTP/1.0";
        case HTTP_VERSION_11:  return "HTTP/1.1";
        case HTTP_VERSION_20:  return "HTTP/2.0";

        default:
            assert(false && "Invalid HTTP version");
            return NULL;
    }
}