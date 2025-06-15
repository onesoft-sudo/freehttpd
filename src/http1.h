#ifndef FHTTPD_HTTP1_H
#define FHTTPD_HTTP1_H

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "protocol.h"

#define HTTP1_PARSER_BUFFER_SIZE 8192
#define HTTP1_RESPONSE_BUFFER_SIZE 8192

#define HTTP1_METHOD_MAX_LEN 16
#define HTTP1_VERSION_MAX_LEN 8
#define HTTP1_URI_MAX_LEN 4096
#define HTTP1_HEADER_NAME_MAX_LEN 256
#define HTTP1_HEADER_VALUE_MAX_LEN 2048
#define HTTP1_HEADERS_MAX_COUNT 256
#define HTTP1_BODY_MAX_LENGTH (128 * 1024 * 1024)

static_assert (HTTP1_PARSER_BUFFER_SIZE >= (HTTP1_METHOD_MAX_LEN + HTTP1_VERSION_MAX_LEN + HTTP1_URI_MAX_LEN + 2));

struct fhttpd_connection;

enum http1_parser_state
{
    HTTP1_STATE_METHOD,
    HTTP1_STATE_URI,
    HTTP1_STATE_VERSION,
    HTTP1_STATE_HEADER_NAME,
    HTTP1_STATE_HEADER_VALUE,
    HTTP1_STATE_BODY,
    HTTP1_STATE_RECV,
    HTTP1_STATE_DONE,
    HTTP1_STATE_ERROR,
};

struct http1_parser_result
{
    bool used;
    enum fhttpd_method method;
    char *uri;
    size_t uri_len;
    char version[4];
    struct fhttpd_headers headers;
    uint64_t content_length;
    char *body;
    size_t body_len;
};

struct http1_parser_ctx
{
    /** TODO: Use a preallocated shared heap memory per worker */
    char buffer[HTTP1_PARSER_BUFFER_SIZE];
    size_t buffer_len;
    enum http1_parser_state state;
    enum http1_parser_state prev_state;

    char last_header_name[HTTP1_HEADER_NAME_MAX_LEN];
    size_t last_header_name_len;

    struct http1_parser_result result;
};

struct http1_response_ctx
{
    bool resline_written;
    size_t written_headers_count;
    bool all_headers_written;
    size_t written_body_len;
    
    char buffer[HTTP1_RESPONSE_BUFFER_SIZE];
    size_t buffer_len;

    bool eos;
    bool drain_first;
};

void http1_parser_ctx_init (struct http1_parser_ctx *ctx);
void http1_parser_ctx_free (struct http1_parser_ctx *ctx);
void http1_response_ctx_init (struct http1_response_ctx *ctx);
void http1_response_ctx_free (struct http1_response_ctx *ctx);
bool http1_parse (struct fhttpd_connection *conn, struct http1_parser_ctx *ctx);
bool http1_response_buffer (struct http1_response_ctx *ctx, struct fhttpd_connection *conn, const struct fhttpd_response *response);

#endif /* FHTTPD_HTTP1_H */

