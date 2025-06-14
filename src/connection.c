#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <unistd.h>

#include "connection.h"
#include "http1.h"
#include "loop.h"
#include "utils.h"

struct fhttpd_connection *
fhttpd_connection_create (uint64_t id, fd_t client_sockfd)
{
    struct fhttpd_connection *conn = calloc (1, sizeof (struct fhttpd_connection));

    if (!conn)
        return NULL;

    conn->id = id;
    conn->client_sockfd = client_sockfd;
    conn->last_recv_timestamp = get_current_timestamp ();
    conn->created_at = conn->last_recv_timestamp;

    return conn;
}

void
fhttpd_connection_free (struct fhttpd_connection *conn)
{
    if (!conn)
        return;

    if (conn->protocol == FHTTPD_PROTOCOL_HTTP1x)
        http1_parser_ctx_free (&conn->http1_ctx);

    if (conn->requests)
    {
        for (size_t i = 0; i < conn->request_count; i++)
        {
            free (conn->requests[i].uri);

            if (conn->requests[i].headers.list)
            {
                for (size_t j = 0; j < conn->requests[i].headers.count; j++)
                {
                    free (conn->requests[i].headers.list[j].name);
                    free (conn->requests[i].headers.list[j].value);
                }

                free (conn->requests[i].headers.list);
            }

            free (conn->requests[i].body);
        }

        free (conn->requests);
    }

    free (conn);
}

ssize_t
fhttpd_connection_recv (struct fhttpd_connection *conn, void *buf, size_t size, int flags)
{
    ssize_t bytes_read = recv (conn->client_sockfd, buf, size, flags);

    if (bytes_read < 0)
        return bytes_read;

    int err = errno;
    conn->last_recv_timestamp = get_current_timestamp ();
    errno = err;

    return bytes_read;
}

bool
fhttpd_connection_detect_protocol (struct fhttpd_connection *conn)
{
    while (conn->buffer_size < H2_PREFACE_SIZE)
    {
        ssize_t bytes_read = fhttpd_connection_recv (conn, conn->buffers.protobuf + conn->buffer_size,
                                                     H2_PREFACE_SIZE - conn->buffer_size, 0);

        if (bytes_read < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;

            return false;
        }
        else if (bytes_read == 0)
        {
            conn->protocol = FHTTPD_PROTOCOL_HTTP1x;
            return true;
        }

        conn->buffer_size += bytes_read;
    }

    if (memcmp (conn->buffers.protobuf, H2_PREFACE, H2_PREFACE_SIZE) == 0)
        conn->protocol = FHTTPD_PROTOCOL_H2;
    else
        conn->protocol = FHTTPD_PROTOCOL_HTTP1x;

    return true;
}

bool
fhttpd_connection_send (struct fhttpd_connection *conn, const void *buf, size_t size, int flags)
{
    ssize_t bytes_sent = send (conn->client_sockfd, buf, size, flags);

    if (bytes_sent < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;

        return false;
    }

    int err = errno;
    conn->last_send_timestamp = get_current_timestamp ();
    errno = err;

    return true;
}

bool
fhttpd_connection_sendfile (struct fhttpd_connection *conn, int src_fd, off_t *offset, size_t count)
{
    ssize_t bytes_sent = sendfile (conn->client_sockfd, src_fd, offset, count);

    if (bytes_sent < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;

        return false;
    }

    int err = errno;
    conn->last_send_timestamp = get_current_timestamp ();
    errno = err;

    return true;
}

bool
fhttpd_connection_error_response (struct fhttpd_connection *conn, enum fhttpd_status code)
{
    const char html_start_title[] = "<!DOCTYPE html>\n"
                                    "<html lang=\"en\">\n"
                                    "<head>\n"
                                    "<meta charset=\"UTF-8\">\n"
                                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                                    "<meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\">\n"
                                    "<style>\n"
                                    "html { font-family: sans-serif; }\n"
                                    "@media (prefers-color-scheme: dark) {\n"
                                    "    html {\n"
                                    "        background: black;\n"
                                    "        color: white;\n"
                                    "    }\n"
                                    "}\n"
                                    "</style>\n"
                                    "<title>";
    const char html_end_title[] = "</title>\n"
                                  "</head>\n"
                                  "<body>\n"
                                  "<h1>";
    const char html_end_heading[] = "</h1>\n";
    const char html_start_paragraph[] = "<p>";
    const char html_end_paragraph[] = "</p>\n";
    const char html_end_body[] = "</body>\n"
                                 "</html>\n";

    const char *status_text = fhttpd_get_status_text (code);
    const char *description = fhttpd_get_status_description (code);

    size_t content_length = (strlen (status_text) * 2) + (2 * 4) + strlen (description) + sizeof (html_start_title)
                            + sizeof (html_end_title) + sizeof (html_end_heading) + sizeof (html_start_paragraph)
                            + sizeof (html_end_paragraph) + sizeof (html_end_body) - 6;

    const char *format = "HTTP/%s %d %s\r\n"
                         "Server: freehttpd\r\n"
                         "Content-Length: %zu\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "%s%d %s%s%d %s%s%s%s%s%s";

    int rc
        = dprintf (conn->client_sockfd, format, conn->exact_protocol[0] == 0 ? "1.1" : conn->exact_protocol, code,
                   status_text, content_length, html_start_title, code, status_text, html_end_title, code, status_text,
                   html_end_heading, html_start_paragraph, description, html_end_paragraph, html_end_body);

    if (rc < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;
            
        return false;
    }

    return true;
}