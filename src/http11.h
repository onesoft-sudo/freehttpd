#ifndef FHTTPD_HTTP11_H
#define FHTTPD_HTTP11_H

#include "protocol.h"
#include "server.h"

#define HTTP11_MAX_REQUEST_LINE_LENGTH 8192
#define HTTP11_MAX_METHOD_LENGTH 16
#define HTTP11_MAX_URI_LENGTH 4096
#define HTTP11_MAX_VERSION_LENGTH 16
#define HTTP11_MAX_HEADERS 100
#define HTTP11_MAX_HEADER_NAME_LENGTH 256
#define HTTP11_MAX_HEADER_VALUE_LENGTH 8192

enum http11_parse_state
{
    HTTP11_PARSE_STATE_START,
    HTTP11_PARSE_STATE_REQUEST_LINE,
    HTTP11_PARSE_STATE_PRE_HEADERS,
    HTTP11_PARSE_STATE_HEADERS,
    HTTP11_PARSE_STATE_BODY,
    HTTP11_PARSE_STATE_COMPLETE,
    HTTP11_PARSE_STATE_ERROR,
};

struct http11_parser_ctx
{
    struct fhttpd_request *request;
    bool headers_complete;
    bool body_complete;
    size_t body_length;
    enum http11_parse_state state;
    char buffer[HTTP11_MAX_REQUEST_LINE_LENGTH];
    size_t buffer_len, buffer_offset;
    enum fhttpd_status suggested_status;
};

enum http11_parse_error
{
    HTTP11_PARSE_ERROR_NONE,
    HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE,
    HTTP11_PARSE_ERROR_INVALID_HEADER,
    HTTP11_PARSE_ERROR_INCOMPLETE_REQUEST,
    HTTP11_PARSE_ERROR_INTERNAL,
    HTTP11_PARSE_ERROR_REPORTED,
    HTTP11_PARSE_ERROR_WAIT,
    HTTP11_PARSE_ERROR_PEER_CLOSED
};

struct http11_parser_ctx;

enum http11_parse_error
http11_stream_parse_request (struct fhttpd_connection *connection,
                             struct http11_parser_ctx *context);

#endif /* FHTTPD_HTTP11_H */