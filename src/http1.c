#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FHTTPD_LOG_MODULE_NAME "http1"

#include "connection.h"
#include "http1.h"
#include "log.h"
#include "utils.h"

#ifdef HAVE_RESOURCES
#include "resources.h"
#endif

#define HTTP1_PARSER_NEXT 0
#define HTTP1_PARSER_RETURN(value) ((1 << 8) | (value))

static const char *http1_method_names[] = {
    [FHTTPD_METHOD_GET] = "GET",       [FHTTPD_METHOD_POST] = "POST",       [FHTTPD_METHOD_PUT] = "PUT",
    [FHTTPD_METHOD_DELETE] = "DELETE", [FHTTPD_METHOD_HEAD] = "HEAD",       [FHTTPD_METHOD_OPTIONS] = "OPTIONS",
    [FHTTPD_METHOD_PATCH] = "PATCH",   [FHTTPD_METHOD_CONNECT] = "CONNECT", [FHTTPD_METHOD_TRACE] = "TRACE",
};

static const size_t http1_method_names_count = sizeof (http1_method_names) / sizeof (http1_method_names[0]);

void
http1_parser_ctx_init (struct http1_parser_ctx *ctx)
{
    memset (ctx, 0, sizeof (struct http1_parser_ctx));
}

void
http1_parser_ctx_free (struct http1_parser_ctx *ctx)
{
    if (!ctx->result.used)
    {
        if (ctx->result.path)
            free (ctx->result.path);

        if (ctx->result.uri)
            free (ctx->result.uri);
        
        if (ctx->result.qs)
            free (ctx->result.qs);

        if (ctx->result.headers.list)
        {
            for (size_t i = 0; i < ctx->result.headers.count; i++)
            {
                free (ctx->result.headers.list[i].name);
                free (ctx->result.headers.list[i].value);
            }

            free (ctx->result.headers.list);
        }

        if (ctx->result.body)
        {
            free (ctx->result.body);
        }
    }
}

static void
buffer_shift (struct http1_parser_ctx *ctx, size_t shift)
{
    if (shift >= ctx->buffer_len)
    {
        fhttpd_wclog_debug ("Buffer shift is greater than or equal to buffer length, resetting buffer");
        ctx->buffer_len = 0;
        return;
    }

    fhttpd_wclog_debug ("Shifting buffer by %zu bytes, current buffer length: %zu", shift, ctx->buffer_len);
    memmove (ctx->buffer, ctx->buffer + shift, ctx->buffer_len - shift);

    ctx->buffer_len -= shift;
    fhttpd_wclog_debug ("Buffer length now: %zu", ctx->buffer_len);
}

static void
http1_push_state (struct http1_parser_ctx *ctx, enum http1_parser_state state)
{
    ctx->prev_state = ctx->state;
    ctx->state = state;
}

static void
http1_pop_state (struct http1_parser_ctx *ctx)
{
    ctx->state = ctx->prev_state;
}

static short
http1_parse_method (struct http1_parser_ctx *ctx)
{
    char *method_end = memchr (ctx->buffer, ' ', ctx->buffer_len);

    if (!method_end)
    {
        http1_push_state (ctx, HTTP1_STATE_RECV);
        return HTTP1_PARSER_NEXT;
    }

    size_t method_len = method_end - ctx->buffer;

    if (method_len == 0 && method_len > HTTP1_METHOD_MAX_LEN)
    {
        fhttpd_wclog_debug ("HTTP1_METHOD_MAX_LEN");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    fhttpd_wclog_debug ("Method: |%.*s|", (int) method_len, ctx->buffer);

    bool method_found = false;

    for (size_t i = 0; i < http1_method_names_count; i++)
    {
        if (strncmp (ctx->buffer, http1_method_names[i], method_len) == 0)
        {
            ctx->result.method = i;
            method_found = true;
            break;
        }
    }

    if (!method_found)
    {
        ctx->state = HTTP1_STATE_ERROR;
        fhttpd_wclog_debug ("!method_found");
        return HTTP1_PARSER_RETURN (false);
    }

    buffer_shift (ctx, method_len + 1);
    fhttpd_wclog_debug ("Parsed method: %d", ctx->result.method);
    ctx->state = HTTP1_STATE_URI;

    return HTTP1_PARSER_NEXT;
}

static short
http1_parse_uri (struct http1_parser_ctx *ctx)
{
    char *uri_end = memchr (ctx->buffer, ' ', ctx->buffer_len);

    if (!uri_end)
    {
        http1_push_state (ctx, HTTP1_STATE_RECV);
        return HTTP1_PARSER_NEXT;
    }

    size_t uri_len = uri_end - ctx->buffer;

    if (uri_len == 0 || uri_len > HTTP1_URI_MAX_LEN)
    {
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    ctx->result.uri = malloc (uri_len + 1);

    if (!ctx->result.uri)
    {
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    memcpy (ctx->result.uri, ctx->buffer, uri_len);

    ctx->result.uri[uri_len] = 0;
    ctx->result.uri_len = uri_len;

    if (ctx->result.uri[0] != '/')
    {
        fhttpd_wclog_error ("Invalid path: %s, relative paths are not allowed", ctx->result.uri);
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    fhttpd_wclog_debug ("URI: |%.*s|", (int) uri_len, ctx->result.uri);

    char *qs_start = memchr (ctx->result.uri, '?', uri_len);

    if (!qs_start)
    {
        ctx->result.qs = NULL;
        ctx->result.qs_len = 0;
        ctx->result.path = strndup (ctx->result.uri, uri_len);
        ctx->result.path_len = uri_len;

        if (!ctx->result.path)
        {
            fhttpd_wclog_debug ("Memory allocation failed for path");
            free (ctx->result.uri);
            ctx->state = HTTP1_STATE_ERROR;
            return HTTP1_PARSER_RETURN (false);
        }
    }
    else
    {
        size_t path_len = qs_start - ctx->result.uri;
        
        ctx->result.path = strndup (ctx->result.uri, path_len);
        ctx->result.path_len = path_len;
        ctx->result.qs = strndup (qs_start + 1, uri_len - path_len - 1);
        ctx->result.qs_len = uri_len - path_len - 1;

        if (!ctx->result.path || !ctx->result.qs)
        {
            fhttpd_wclog_debug ("Memory allocation failed for path or query string");
            free (ctx->result.path);
            free (ctx->result.qs);
            ctx->result.path = NULL;
            ctx->result.qs = NULL;
            ctx->state = HTTP1_STATE_ERROR;
            return HTTP1_PARSER_RETURN (false);
        }
    }

    fhttpd_wclog_debug ("Path: |%.*s|, Query String: |%.*s|", (int) ctx->result.path_len, ctx->result.path,
                        (int) ctx->result.qs_len, ctx->result.qs);

    buffer_shift (ctx, uri_len + 1);
    ctx->state = HTTP1_STATE_VERSION;

    return HTTP1_PARSER_NEXT;
}

static short
http1_parse_version (struct http1_parser_ctx *ctx)
{
    char *version_end = memchr (ctx->buffer, '\n', ctx->buffer_len);

    if (!version_end)
    {
        http1_push_state (ctx, HTTP1_STATE_RECV);
        return HTTP1_PARSER_NEXT;
    }

    size_t version_len = version_end - ctx->buffer;

    if (version_len < 9 || version_len > HTTP1_VERSION_MAX_LEN + 1 || ctx->buffer_len < 9)
    {
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    if (ctx->buffer[version_len - 1] != '\r')
    {
        fhttpd_wclog_debug ("HTTP version does not end with CRLF, rejecting request");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    version_len--;

    if (memcmp (ctx->buffer, "HTTP/", 5) != 0)
    {
        fhttpd_wclog_debug ("Invalid HTTP version: does not start with 'HTTP/', rejecting request");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    char *version_str = ctx->buffer + 5;
    char c1 = version_str[0], c2 = version_str[1], c3 = version_str[2];

    if (c2 != '.' || !isdigit (c1) || !isdigit (c3))
    {
        fhttpd_wclog_debug ("Invalid HTTP version format, rejecting request");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    if (c1 != '1' || (c3 != '0' && c3 != '1'))
    {
        fhttpd_wclog_debug ("Unsupported HTTP version: %c.%c, rejecting request", c1, c3);
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    ctx->result.version[0] = c1;
    ctx->result.version[1] = '.';
    ctx->result.version[2] = c3;
    ctx->result.version[3] = 0;

    fhttpd_wclog_debug ("HTTP version: |%s|", ctx->result.version);

    buffer_shift (ctx, version_len + 2);
    ctx->state = HTTP1_STATE_HEADER_NAME;

    return HTTP1_PARSER_NEXT;
}

static short
http1_parse_header_name (struct http1_parser_ctx *ctx)
{
    if (ctx->buffer_len >= 2 && ctx->buffer[0] == '\r' && ctx->buffer[1] == '\n')
    {
        fhttpd_wclog_debug ("EOH detected, switching to body state");
        buffer_shift (ctx, 2);
        ctx->state = HTTP1_STATE_BODY;
        return HTTP1_PARSER_NEXT;
    }

    char *hname_end = memchr (ctx->buffer, ':', ctx->buffer_len);

    if (!hname_end)
    {
        http1_push_state (ctx, HTTP1_STATE_RECV);
        return HTTP1_PARSER_NEXT;
    }

    size_t hname_len = hname_end - ctx->buffer;

    if (hname_len == 0 || hname_len > HTTP1_HEADER_NAME_MAX_LEN)
    {
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    if (ctx->result.headers.count >= HTTP1_HEADERS_MAX_COUNT)
    {
        fhttpd_wclog_debug ("Too many headers, rejecting request");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    if (!fhttpd_validate_header_name (ctx->buffer, hname_len))
    {
        fhttpd_wclog_debug ("Header name validation failed: Invalid name");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    fhttpd_wclog_debug ("Header name: |%.*s|", (int) hname_len, ctx->buffer);

    ctx->last_header_name_len = hname_len;
    memcpy (ctx->last_header_name, ctx->buffer, hname_len);

    buffer_shift (ctx, hname_len + 1);
    ctx->state = HTTP1_STATE_HEADER_VALUE;

    return HTTP1_PARSER_NEXT;
}

static short
http1_parse_header_value (struct http1_parser_ctx *ctx)
{
    char *hvalue_end = memchr (ctx->buffer, '\n', ctx->buffer_len);

    if (!hvalue_end)
    {
        http1_push_state (ctx, HTTP1_STATE_RECV);
        return HTTP1_PARSER_NEXT;
    }

    size_t hvalue_len = hvalue_end - ctx->buffer;

    if (hvalue_len < 1 || hvalue_len > HTTP1_HEADER_VALUE_MAX_LEN)
    {
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    if (ctx->buffer[hvalue_len - 1] != '\r')
    {
        fhttpd_wclog_debug ("Header value does not end with CRLF, rejecting request");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    hvalue_len--;

    size_t hvalue_str_len = 0;
    char *hvalue_str = str_trim_whitespace (ctx->buffer, hvalue_len, &hvalue_str_len);

    if (!hvalue_str || hvalue_str_len == 0)
    {
        fhttpd_wclog_debug ("Header value is empty, contains only spaces or invalid, rejecting request");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    char *hname_str = strndup (ctx->last_header_name, ctx->last_header_name_len);

    if (!hname_str)
    {
        free (hvalue_str);
        fhttpd_wclog_debug ("Memory allocation failed");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    size_t count = ctx->result.headers.count;
    struct fhttpd_header *list = realloc (ctx->result.headers.list, sizeof (struct fhttpd_header) * (count + 1));

    if (!list)
    {
        free (hname_str);
        free (hvalue_str);
        fhttpd_wclog_debug ("Memory allocation failed");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    ctx->result.headers.list = list;
    list[count].name = hname_str;
    list[count].value = hvalue_str;
    list[count].name_length = ctx->last_header_name_len;
    list[count].value_length = hvalue_str_len;
    ctx->result.headers.count++;

    if (strcasecmp (hname_str, "Content-Length") == 0)
    {
        char *endptr;
        uint64_t content_length = strtoull (hvalue_str, &endptr, 10);

        if (*endptr)
        {
            fhttpd_wclog_debug ("Invalid Content-Length header value: %s", hvalue_str);
            ctx->state = HTTP1_STATE_ERROR;
            return HTTP1_PARSER_RETURN (false);
        }

        ctx->result.content_length = content_length;
    }
    else if (strcasecmp (hname_str, "Host") == 0)
    {
        uint16_t host_port = 80; /** FIXME: TLS */
        char *colon = memchr (hvalue_str, ':', hvalue_str_len);
        char *endptr = NULL;

        if (!colon) 
        {
            ctx->result.host = strndup (hvalue_str, hvalue_str_len);
            ctx->result.host_len = hvalue_str_len;
            ctx->result.host_port = host_port;
        }
        else
        {
            ctx->result.host_len = colon - hvalue_str;
            ctx->result.host = NULL;
            host_port = (uint16_t) strtoul (colon + 1, &endptr, 10);

            if (*endptr)
            {
                fhttpd_wclog_debug ("Invalid Host header value: %s", hvalue_str);
                ctx->state = HTTP1_STATE_ERROR;
                return HTTP1_PARSER_RETURN (false);
            }

            ctx->result.host = strndup (hvalue_str, ctx->result.host_len);
            ctx->result.host_port = host_port;
        }
    }

    fhttpd_wclog_debug ("Header value: |%.*s|", (int) hvalue_str_len, hvalue_str);
    buffer_shift (ctx, hvalue_len + 2);
    ctx->state = HTTP1_STATE_HEADER_NAME;

    return HTTP1_PARSER_NEXT;
}

static short
http1_parse_body (struct http1_parser_ctx *ctx)
{
    if (ctx->result.method == FHTTPD_METHOD_GET || ctx->result.method == FHTTPD_METHOD_HEAD
        || ctx->result.method == FHTTPD_METHOD_TRACE)
    {
        fhttpd_wclog_debug ("No body expected for method %s, switching to done state",
                            fhttpd_method_to_string (ctx->result.method));
        ctx->state = HTTP1_STATE_DONE;
        return HTTP1_PARSER_NEXT;
    }

    if (ctx->result.content_length == 0)
    {
        fhttpd_wclog_debug ("No content-length header, switching to done state");
        ctx->state = HTTP1_STATE_DONE;
        return HTTP1_PARSER_NEXT;
    }

    if (ctx->buffer_len == 0)
    {
        http1_push_state (ctx, HTTP1_STATE_RECV);
        return HTTP1_PARSER_NEXT;
    }

    if (ctx->result.body_len + ctx->buffer_len > HTTP1_BODY_MAX_LENGTH)
    {
        free (ctx->result.body);

        ctx->result.body_len = 0;
        ctx->result.body = NULL;

        fhttpd_wclog_debug ("Body length exceeds limit, rejecting request");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    uint64_t content_length = ctx->result.content_length;
    size_t next_len = ctx->result.body_len + ctx->buffer_len > content_length ? content_length - ctx->result.body_len
                                                                              : ctx->buffer_len;

    char *body = realloc (ctx->result.body, ctx->result.body_len + next_len);

    if (!body)
    {
        fhttpd_wclog_debug ("Memory allocation failed for body");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    ctx->result.body = body;
    memcpy (ctx->result.body + ctx->result.body_len, ctx->buffer, next_len);

    ctx->result.body_len += next_len;

    if (next_len >= ctx->buffer_len)
        ctx->buffer_len = 0;
    else
        ctx->buffer_len -= next_len;

    if (ctx->result.body_len >= ctx->result.content_length)
    {
        fhttpd_wclog_debug ("Body fully received (%zu bytes), switching to done state", ctx->result.body_len);
        ctx->state = HTTP1_STATE_DONE;
        return HTTP1_PARSER_NEXT;
    }

    fhttpd_wclog_debug ("Body partially received, waiting for more data");
    return HTTP1_PARSER_NEXT;
}

static short
http1_parse_recv (struct fhttpd_connection *conn, struct http1_parser_ctx *ctx)
{
    if (ctx->buffer_len >= HTTP1_PARSER_BUFFER_SIZE)
    {
        fhttpd_wclog_debug ("Buffer full, this should not happen in a well-formed request");
        fhttpd_wclog_debug ("Rejecting this request");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    fhttpd_wclog_debug ("Receiving more data, current buffer length: %zu", ctx->buffer_len);

    ssize_t bytes_received
        = fhttpd_connection_recv (conn, ctx->buffer + ctx->buffer_len, HTTP1_PARSER_BUFFER_SIZE - ctx->buffer_len, 0);

    if (bytes_received == 0)
    {
        ctx->state = HTTP1_STATE_ERROR;
        fhttpd_wclog_debug ("Connection closed by peer, rejecting request: %s", strerror (errno));
        return HTTP1_PARSER_RETURN (false);
    }

    if (bytes_received < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            fhttpd_wclog_debug ("No more data available, waiting for more data");
            return HTTP1_PARSER_RETURN (true);
        }

        int err = errno;
        fhttpd_wclog_debug ("Error receiving data: %s", strerror (errno));
        ctx->state = HTTP1_STATE_ERROR;
        errno = err;
        return HTTP1_PARSER_RETURN (false);
    }

    if (ctx->buffer_len + bytes_received >= HTTP1_PARSER_BUFFER_SIZE)
    {
        fhttpd_wclog_debug ("Buffer would overflow, rejecting request");
        ctx->state = HTTP1_STATE_ERROR;
        return HTTP1_PARSER_RETURN (false);
    }

    fhttpd_wclog_debug ("Received %zd bytes, total buffer length: %zu", bytes_received,
                        ctx->buffer_len + bytes_received);

    ctx->buffer_len += (size_t) bytes_received;
    http1_pop_state (ctx);
    return HTTP1_PARSER_NEXT;
}

bool
http1_parse (struct fhttpd_connection *conn, struct http1_parser_ctx *ctx)
{
    ctx->processing = true;
    
    while (true)
    {
        short result = HTTP1_PARSER_NEXT;

        switch (ctx->state)
        {
            case HTTP1_STATE_METHOD:
                result = http1_parse_method (ctx);
                break;

            case HTTP1_STATE_URI:
                result = http1_parse_uri (ctx);
                break;

            case HTTP1_STATE_VERSION:
                result = http1_parse_version (ctx);
                break;

            case HTTP1_STATE_HEADER_NAME:
                result = http1_parse_header_name (ctx);
                break;

            case HTTP1_STATE_HEADER_VALUE:
                result = http1_parse_header_value (ctx);
                break;

            case HTTP1_STATE_BODY:
                result = http1_parse_body (ctx);
                break;

            case HTTP1_STATE_RECV:
                result = http1_parse_recv (conn, ctx);
                break;

            case HTTP1_STATE_DONE:
                ctx->processing = false;
                return true;

            case HTTP1_STATE_ERROR:
                fhttpd_wclog_debug ("Parser error, rejecting request");
                ctx->processing = false;
                return false;

            default:
                fhttpd_wclog_debug ("Unknown parser state: %d", ctx->state);
                ctx->processing = false;
                ctx->state = HTTP1_STATE_ERROR;
                return false;
        }

        if (result == HTTP1_PARSER_NEXT)
            continue;

        if ((result >> 8) == 1)
            return (bool) (result & 0xFF);

        fhttpd_wclog_debug ("Invalid return value: %hd", result);
        ctx->state = HTTP1_STATE_ERROR;
        ctx->processing = false;
        return false;
    }

    return true;
}

bool
http1_response_buffer (struct http1_response_ctx *ctx, struct fhttpd_connection *conn, const struct fhttpd_response *response)
{
    if (ctx->eos)
        return false;

    if (ctx->drain_first && ctx->buffer_len > 0)
        return true;

    ctx->drain_first = false;

    while (ctx->buffer_len < HTTP1_RESPONSE_BUFFER_SIZE)
    {
        if (!ctx->resline_written)
        {
            int sp_bytes = snprintf (ctx->buffer, HTTP1_RESPONSE_BUFFER_SIZE, "HTTP/%3s %d %s\r\n",
                                     conn->exact_protocol[0] == 0 ? "1.1" : conn->exact_protocol, response->status,
                                     fhttpd_get_status_text (response->status));

            if (sp_bytes <= 0)
                return false;

            if (ctx->buffer_len + sp_bytes > HTTP1_RESPONSE_BUFFER_SIZE)
            {
                fhttpd_wclog_debug ("Response line too long, this should not happen");
                return false;
            }

            ctx->resline_written = true;
            ctx->buffer_len += sp_bytes;
            continue;
        }

        if (ctx->written_headers_count < response->headers.count)
        {
            const struct fhttpd_header *header = &response->headers.list[ctx->written_headers_count];

            size_t name_len = header->name_length;
            size_t value_len = header->value_length;
            size_t total = name_len + value_len + 4;

            if (ctx->buffer_len + total > HTTP1_RESPONSE_BUFFER_SIZE)
            {
                ctx->drain_first = true;
                return true;
            }

            memcpy (ctx->buffer + ctx->buffer_len, header->name, name_len);
            memcpy (ctx->buffer + ctx->buffer_len + name_len, ": ", 2);
            memcpy (ctx->buffer + ctx->buffer_len + name_len + 2, header->value, value_len);
            memcpy (ctx->buffer + ctx->buffer_len + name_len + 2 + value_len, "\r\n", 2);

            ctx->buffer_len += total;
            ctx->written_headers_count++;

            fhttpd_wclog_debug ("Added header: %.*s: %.*s", (int) name_len, header->name, (int) value_len,
                                header->value);
            continue;
        }

        if (!ctx->all_headers_written)
        {
            if (response->set_content_length)
            {
                char header_buf[65] = {0};
                int sp_bytes
                    = snprintf (header_buf, sizeof (header_buf) - 1, "Content-Length: %lu\r\n", response->body_len);

                if (sp_bytes <= 0)
                    return false;

                if (header_buf[63] == '\r' || header_buf[64] != 0 || (size_t) sp_bytes >= sizeof (header_buf) - 1)
                {
                    fhttpd_wclog_debug ("Header buffer overflow");
                    return false;
                }

                if (ctx->buffer_len + sp_bytes > HTTP1_RESPONSE_BUFFER_SIZE)
                {
                    ctx->drain_first = true;
                    return true;
                }

                memcpy (ctx->buffer + ctx->buffer_len, header_buf, sp_bytes);

                ctx->written_headers_count++;
                ctx->buffer_len += sp_bytes;

                fhttpd_wclog_debug ("Added content length header");
            }

            ctx->all_headers_written = true;
            continue;
        }

        if (response->use_builtin_error_response)
        {
            uint16_t port = conn->port;
            const char *host = conn->hostname ? conn->hostname : "0.0.0.0";
            size_t host_len = strlen (host);
            size_t port_width = port >= 10000 ? 5 : port >= 1000 ? 4 : port >= 100 ? 3 : port >= 10 ? 2 : 1;

            const char *status_text = fhttpd_get_status_text (response->status);
            const char *description = fhttpd_get_status_description (response->status);
            const size_t content_length
                = (strlen (status_text) * 2) + (2 * 3) + host_len + port_width + strlen (description) + resource_error_html_len - 14;

            if (ctx->buffer_len + content_length + 104 + host_len + port_width > HTTP1_RESPONSE_BUFFER_SIZE)
            {
                ctx->drain_first = true;
                return true;
            }

            char format[64 + resource_error_html_len + 1];

            memcpy (format, "Content-Type: text/html; charset=UTF-8\r\nContent-Length: %zu\r\n\r\n", 63);
            memcpy (format + 63, (const char *) resource_error_html, resource_error_html_len);

            format[63 + resource_error_html_len] = 0;

            int sp_bytes
                = snprintf (ctx->buffer + ctx->buffer_len, HTTP1_RESPONSE_BUFFER_SIZE - ctx->buffer_len, format,
                            content_length, response->status, status_text, response->status, status_text, description, host, port);

            if (sp_bytes <= 0)
                return false;

            if (ctx->buffer_len + sp_bytes > HTTP1_RESPONSE_BUFFER_SIZE)
            {
                ctx->drain_first = true;
                return true;
            }

            ctx->buffer_len += sp_bytes;
            ctx->eos = true;
            fhttpd_wclog_debug ("Added built-in error response");
            break;
        }

        if (response->body && response->body_len > 0 && ctx->written_body_len < response->body_len)
        {
            if (ctx->written_body_len == 0)
            {
                if (ctx->buffer_len + 2 >= HTTP1_RESPONSE_BUFFER_SIZE)
                {
                    ctx->drain_first = true;
                    return true;
                }

                memcpy (ctx->buffer + ctx->buffer_len, "\r\n", 2);
                ctx->buffer_len += 2;
            }

            if (ctx->buffer_len >= HTTP1_RESPONSE_BUFFER_SIZE)
            {
                ctx->drain_first = true;
                return true;
            }

            size_t remaining = response->body_len - ctx->written_body_len;
            size_t to_write = remaining < (HTTP1_RESPONSE_BUFFER_SIZE - ctx->buffer_len)
                                  ? remaining
                                  : (HTTP1_RESPONSE_BUFFER_SIZE - ctx->buffer_len);
            
            if (to_write == 0 || ctx->written_body_len > response->body_len)
            {
                fhttpd_wclog_debug ("No more body to write or already written all body, stopping");
                ctx->eos = true;
                break;
            }

            memcpy (ctx->buffer + ctx->buffer_len, response->body + ctx->written_body_len, to_write);

            ctx->buffer_len += to_write;
            ctx->written_body_len += to_write;
            continue;
        }

        ctx->eos = true;
        break;
    }

    return true;
}

void
http1_response_ctx_init (struct http1_response_ctx *ctx)
{
    memset (ctx, 0, sizeof (struct http1_response_ctx));
}

void
http1_response_ctx_free (struct http1_response_ctx *ctx)
{
    if (ctx->fd > 0)
    {
        fhttpd_log_debug ("Closing file descriptor %d in response context", ctx->fd);
        close (ctx->fd);
        ctx->fd = -1;
    }
}