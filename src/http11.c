#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http11.h"
#include "server.h"

static const char *http11_method_strings[] = {
    [FHTTPD_METHOD_GET] = "GET",     [FHTTPD_METHOD_POST] = "POST",
    [FHTTPD_METHOD_PUT] = "PUT",     [FHTTPD_METHOD_DELETE] = "DELETE",
    [FHTTPD_METHOD_HEAD] = "HEAD",   [FHTTPD_METHOD_OPTIONS] = "OPTIONS",
    [FHTTPD_METHOD_PATCH] = "PATCH", [FHTTPD_METHOD_CONNECT] = "CONNECT",
    [FHTTPD_METHOD_TRACE] = "TRACE",
};

static const size_t http11_method_string_count
    = sizeof (http11_method_strings) / sizeof (http11_method_strings[0]);

static enum http11_parse_error
http11_parse_request_line (
    struct fhttpd_connection *connection __attribute_maybe_unused__,
    struct http11_parser_ctx *context, size_t request_line_length)
{
    char *buffer = context->buffer;
    char method[HTTP11_MAX_METHOD_LENGTH + 1];
    char uri[HTTP11_MAX_URI_LENGTH + 1];
    char version[HTTP11_MAX_VERSION_LENGTH + 1];
    size_t method_length = 0, uri_length = 0, version_length = 0;
    size_t i = 0;

    while (i < request_line_length && buffer[i] != ' ')
    {
        if (method_length >= HTTP11_MAX_METHOD_LENGTH)
        {
            context->state = HTTP11_PARSE_STATE_ERROR;
            context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
            return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
        }

        method[method_length++] = buffer[i++];
    }

    method[method_length] = 0;

    if (buffer[i] != ' ')
    {
        context->state = HTTP11_PARSE_STATE_ERROR;
        context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
        return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
    }

    i++;

    while (i < request_line_length && buffer[i] != ' ')
    {
        if (uri_length >= HTTP11_MAX_URI_LENGTH)
        {
            context->state = HTTP11_PARSE_STATE_ERROR;
            context->suggested_status = FHTTPD_STATUS_REQUEST_URI_TOO_LONG;
            return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
        }

        uri[uri_length++] = buffer[i++];
    }

    uri[uri_length] = 0;

    if (buffer[i] != ' ')
    {
        context->state = HTTP11_PARSE_STATE_ERROR;
        context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
        return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
    }

    i++;

    while (i < request_line_length && buffer[i] != '\r' && buffer[i] != '\n')
    {
        if (version_length >= HTTP11_MAX_VERSION_LENGTH)
        {
            context->state = HTTP11_PARSE_STATE_ERROR;
            context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
            return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
        }

        version[version_length++] = buffer[i++];
    }

    version[version_length] = 0;

    if (method_length == 0 || uri_length == 0 || version_length == 0)
    {
        context->state = HTTP11_PARSE_STATE_ERROR;
        context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
        return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
    }

    bool method_set = false;

    for (size_t j = 0; j < http11_method_string_count; j++)
    {
        if (strcmp (method, http11_method_strings[j]) == 0)
        {
            context->request->method = j;
            method_set = true;
            break;
        }
    }

    if (!method_set)
    {
        context->state = HTTP11_PARSE_STATE_ERROR;
        context->suggested_status = FHTTPD_STATUS_NOT_IMPLEMENTED;
        return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
    }

    if (strncmp (version, "HTTP/1.1", 8) != 0
        && strncmp (version, "HTTP/1.0", 8) != 0)
    {
        context->state = HTTP11_PARSE_STATE_ERROR;
        context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
        return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
    }

    context->request->uri = strndup (uri, uri_length);

    if (!context->request->uri)
    {
        context->state = HTTP11_PARSE_STATE_ERROR;
        context->suggested_status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
        return HTTP11_PARSE_ERROR_INTERNAL;
    }

    context->request->uri_length = uri_length;
    context->request->protocol = FHTTPD_PROTOCOL_HTTP1x;
    return HTTP11_PARSE_ERROR_NONE;
}

static bool
buffer_shift (struct http11_parser_ctx *context, size_t shift)
{
    if (shift >= context->buffer_len)
    {
        context->buffer_len = 0;
        return true;
    }

    memmove (context->buffer, context->buffer + shift,
             context->buffer_len - shift);

    context->buffer_len -= shift;
    context->buffer_offset = 0;

    return false;
}

enum http11_parse_error
http11_stream_parse_request (struct fhttpd_connection *connection,
                             struct http11_parser_ctx *context)
{
    switch (context->state)
    {
        case HTTP11_PARSE_STATE_START:
            context->state = HTTP11_PARSE_STATE_REQUEST_LINE;
            break;

        case HTTP11_PARSE_STATE_REQUEST_LINE:
            {
                if (connection->buffer_len != 0 && context->buffer_len == 0)
                {
                    memcpy (context->buffer, connection->buffer,
                            connection->buffer_len);
                    context->buffer_len = connection->buffer_len;
                }

                bool eagain = false;

                while (true)
                {
                    ssize_t bytes_read = fhttpd_connection_recv (
                        connection, context->buffer + context->buffer_len,
                        HTTP11_MAX_REQUEST_LINE_LENGTH - context->buffer_len,
                        0);

                    if (bytes_read < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            eagain = true;
                            break;
                        }
                        else
                        {
                            context->state = HTTP11_PARSE_STATE_ERROR;
                            context->suggested_status
                                = FHTTPD_STATUS_BAD_REQUEST;
                            return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
                        }
                    }
                    else if (bytes_read == 0)
                    {
                        context->state = HTTP11_PARSE_STATE_ERROR;
                        return HTTP11_PARSE_ERROR_PEER_CLOSED;
                    }
                    else if (context->buffer_len + bytes_read
                             >= HTTP11_MAX_REQUEST_LINE_LENGTH)
                        break;

                    context->buffer_len += bytes_read;
                }

                char *eol = memchr (context->buffer, '\n', context->buffer_len);

                if (!eol)
                {
                    if (eagain)
                        return HTTP11_PARSE_ERROR_WAIT;

                    context->state = HTTP11_PARSE_STATE_ERROR;
                    context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                    return HTTP11_PARSE_ERROR_INCOMPLETE_REQUEST;
                }

                if (eol - 1 < context->buffer || *(eol - 1) != '\r')
                {
                    context->state = HTTP11_PARSE_STATE_ERROR;
                    context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                    return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
                }

                size_t request_line_length = eol - context->buffer - 1;

                if (request_line_length == 0
                    || request_line_length >= HTTP11_MAX_REQUEST_LINE_LENGTH)
                {
                    context->state = HTTP11_PARSE_STATE_ERROR;
                    context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                    return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
                }

                enum http11_parse_error error = http11_parse_request_line (
                    connection, context, request_line_length);

                if (error != HTTP11_PARSE_ERROR_NONE)
                {
                    context->state = HTTP11_PARSE_STATE_ERROR;
                    return error;
                }

                context->buffer_offset = request_line_length;

                if (context->buffer_offset == context->buffer_len)
                {
                    context->buffer_len = 0;
                    context->buffer_offset = 0;
                }
                else if (context->buffer_offset > context->buffer_len)
                {
                    context->state = HTTP11_PARSE_STATE_ERROR;
                    context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                    return HTTP11_PARSE_ERROR_INVALID_REQUEST_LINE;
                }

                buffer_shift (context, context->buffer_offset);

                context->state = HTTP11_PARSE_STATE_PRE_HEADERS;
                context->headers_complete = false;
            }

            break;

        case HTTP11_PARSE_STATE_PRE_HEADERS:
            if (context->buffer_len < 2)
            {
                ssize_t bytes_read = fhttpd_connection_recv (
                    connection, context->buffer + context->buffer_len,
                    HTTP11_MAX_REQUEST_LINE_LENGTH - context->buffer_len, 0);

                if (bytes_read < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        if (context->buffer_len == 0)
                            return HTTP11_PARSE_ERROR_WAIT;

                        break;
                    }
                    else
                    {
                        context->state = HTTP11_PARSE_STATE_ERROR;
                        context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                        return HTTP11_PARSE_ERROR_INVALID_HEADER;
                    }
                }
                else if (bytes_read == 0)
                {
                    context->state = HTTP11_PARSE_STATE_ERROR;
                    return HTTP11_PARSE_ERROR_PEER_CLOSED;
                }

                context->buffer_len += bytes_read;

                if (context->buffer_len < 2)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return HTTP11_PARSE_ERROR_WAIT;

                    context->state = HTTP11_PARSE_STATE_ERROR;
                    context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                    return HTTP11_PARSE_ERROR_INVALID_HEADER;
                }
            }

            if (context->buffer[0] == '\r' && context->buffer[1] == '\n')
            {
                context->state = HTTP11_PARSE_STATE_HEADERS;
                buffer_shift (context, 2);
                return HTTP11_PARSE_ERROR_NONE;
            }

            break;

        case HTTP11_PARSE_STATE_HEADERS:
            {
                do
                {
                    while (true)
                    {
                        char *eol = context->buffer_len == 0
                                        ? NULL
                                        : memchr (context->buffer, '\n',
                                                  context->buffer_len);

                        if (eol)
                        {
                            if (eol - 1 < context->buffer || *(eol - 1) != '\r')
                            {
                                context->state = HTTP11_PARSE_STATE_ERROR;
                                context->suggested_status
                                    = FHTTPD_STATUS_BAD_REQUEST;
                                return HTTP11_PARSE_ERROR_INVALID_HEADER;
                            }

                            size_t header_length = eol - context->buffer - 1;

                            if (header_length == 0)
                            {
                                context->headers_complete = true;
                                context->state = HTTP11_PARSE_STATE_COMPLETE;
                                buffer_shift (context, context->buffer_offset);
                                return HTTP11_PARSE_ERROR_NONE;
                            }

                            char header[HTTP11_MAX_HEADER_NAME_LENGTH + 1];
                            memcpy (header, context->buffer, header_length);
                            // printf ("Header: %.*s\n", (int) header_length,
                            //         header);
                            buffer_shift (context, header_length + 2);
                        }
                        else
                        {
                            break;
                        }
                    }

                    ssize_t bytes_read = fhttpd_connection_recv (
                        connection, context->buffer + context->buffer_len,
                        HTTP11_MAX_REQUEST_LINE_LENGTH - context->buffer_len,
                        0);

                    if (bytes_read < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            return HTTP11_PARSE_ERROR_WAIT;
                        }
                        else
                        {
                            context->state = HTTP11_PARSE_STATE_ERROR;
                            context->suggested_status
                                = FHTTPD_STATUS_BAD_REQUEST;
                            return HTTP11_PARSE_ERROR_INVALID_HEADER;
                        }
                    }
                    else if (bytes_read == 0)
                    {
                        context->state = HTTP11_PARSE_STATE_ERROR;
                        return HTTP11_PARSE_ERROR_INCOMPLETE_REQUEST;
                    }
                    else if (context->buffer_len + bytes_read
                             >= HTTP11_MAX_REQUEST_LINE_LENGTH)
                    {
                        context->state = HTTP11_PARSE_STATE_ERROR;
                        context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                        return HTTP11_PARSE_ERROR_INVALID_HEADER;
                    }

                    context->buffer_len += bytes_read;
                } while (true);
            }
            break;

        case HTTP11_PARSE_STATE_ERROR:
            return HTTP11_PARSE_ERROR_REPORTED;

        default:
            assert (false && "Invalid parse state");
            return -1;
    }

    return HTTP11_PARSE_ERROR_NONE;
}