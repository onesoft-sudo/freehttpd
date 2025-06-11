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

enum http11_parse_error
http11_stream_parse_request (struct fhttpd_connection *connection,
                             struct http11_parser_ctx *context)
{
    switch (context->state)
    {
        case HTTP11_PARSE_STATE_START:
            context->state = HTTP11_PARSE_STATE_METHOD;
            break;

        case HTTP11_PARSE_STATE_METHOD:
            {
                if (connection->buffer_len != 0 && context->buffer_len == 0)
                {
                    memcpy (context->buffer, connection->buffer,
                            connection->buffer_len);
                    context->buffer_len = connection->buffer_len;
                }

            parse_method:
                {
                    char *method_end
                        = memchr (context->buffer, ' ', context->buffer_len);

                    if (method_end)
                    {
                        size_t method_length = method_end - context->buffer;
                        char method[HTTP11_MAX_METHOD_LENGTH + 1] = { 0 };

                        if (method_length > HTTP11_MAX_METHOD_LENGTH)
                        {
                            context->state = HTTP11_PARSE_STATE_ERROR;
                            context->suggested_status
                                = FHTTPD_STATUS_BAD_REQUEST;
                            return HTTP11_PARSE_ERROR_INVALID_METHOD;
                        }

                        memcpy (method, context->buffer, method_length);
                        method[method_length] = 0;

                        for (size_t i = 0; i < http11_method_string_count; i++)
                        {
                            if (strcmp (method, http11_method_strings[i]) == 0)
                            {
                                context->request->method = i;
                                context->state = HTTP11_PARSE_STATE_URI;
                                context->buffer_offset = method_length + 1;
                                return HTTP11_PARSE_ERROR_NONE;
                            }
                        }

                        context->state = HTTP11_PARSE_STATE_ERROR;
                        context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                        return HTTP11_PARSE_ERROR_INVALID_METHOD;
                    }

                    ssize_t bytes_read = fhttpd_connection_recv (
                        connection, context->buffer + context->buffer_offset,
                        sizeof (context->buffer) - context->buffer_offset, 0);

                    if (bytes_read < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            return HTTP11_PARSE_ERROR_NONE;

                        context->state = HTTP11_PARSE_STATE_ERROR;
                        context->suggested_status
                            = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
                        return HTTP11_PARSE_ERROR_INTERNAL;
                    }
                    else if (bytes_read == 0)
                    {
                        context->state = HTTP11_PARSE_STATE_ERROR;
                        context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                        return HTTP11_PARSE_ERROR_INCOMPLETE_REQUEST;
                    }

                    context->buffer_len += bytes_read;

                    if (context->buffer_len >= HTTP11_MAX_REQUEST_LINE_LENGTH)
                    {
                        context->state = HTTP11_PARSE_STATE_ERROR;
                        context->suggested_status = FHTTPD_STATUS_BAD_REQUEST;
                        return HTTP11_PARSE_ERROR_INVALID_METHOD;
                    }

                    goto parse_method;
                }
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