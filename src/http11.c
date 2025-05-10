#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#include "http11.h"
#include "server.h"
#include "protocol.h"
#include "buffer.h"

#define BUFFER_SIZE 1024

enum http_status_line_parse_state
{
    HTTP_STATUS_LINE_METHOD,
    HTTP_STATUS_LINE_URI,
    HTTP_STATUS_LINE_VERSION,
    HTTP_STATUS_LINE_DONE
};

enum http_parse_state
{
    HTTP_PARSE_HEADER_NAME,
    HTTP_PARSE_HEADER_VALUE,
    HTTP_PARSE_DONE
};

enum http_response_code http11_parse_status_line(struct fhttpd_server *server __attribute_maybe_unused__,
                                                 int client_sockfd, struct http_request *request,
                                                 char **extra_data, size_t *extra_data_length)
{
    enum http_status_line_parse_state state = HTTP_STATUS_LINE_METHOD;
    char buffer[BUFFER_SIZE];

    char method[HTTP_METHOD_MAX_LENGTH + 1],
        uri[HTTP_URI_MAX_LENGTH + 1],
        version[HTTP_VERSION_MAX_LENGTH + 1];
    size_t method_length = 0,
           uri_length = 0,
           version_length = 0;

    ssize_t bytes_received, i = 0;

    *extra_data = NULL;
    bytes_received = recv(client_sockfd, buffer, sizeof(buffer), 0);

    if (bytes_received < 0)
        return HTTP_BAD_REQUEST;

    while (i < bytes_received)
    {
        switch (state)
        {
        case HTTP_STATUS_LINE_METHOD:
            if (method_length > HTTP_METHOD_MAX_LENGTH)
                return HTTP_BAD_REQUEST;

            if (buffer[i] == ' ')
            {
                method[method_length] = 0;
                state = HTTP_STATUS_LINE_URI;
                goto continue_loop;
            }

            method[method_length++] = buffer[i];
            break;

        case HTTP_STATUS_LINE_URI:
            if (uri_length > HTTP_URI_MAX_LENGTH)
                return HTTP_BAD_REQUEST;

            if (buffer[i] == ' ')
            {
                uri[uri_length] = 0;
                state = HTTP_STATUS_LINE_VERSION;
                goto continue_loop;
            }

            uri[uri_length++] = buffer[i];
            break;

        case HTTP_STATUS_LINE_VERSION:
            if (version_length > HTTP_VERSION_MAX_LENGTH)
                return HTTP_BAD_REQUEST;

            if (buffer[i] == '\r' || buffer[i] == '\n')
            {
                version[version_length] = 0;
                state = HTTP_STATUS_LINE_DONE;
                goto continue_loop;
            }

            version[version_length++] = buffer[i];
            break;

        default:
            assert(false && "Invalid state");
            return HTTP_INTERNAL_SERVER_ERROR;
        }

    continue_loop:
        if (state == HTTP_STATUS_LINE_DONE)
        {
            if (i + 1 < bytes_received && buffer[i] == '\r' && buffer[i + 1] == '\n')
                i += 2;

            *extra_data = malloc(bytes_received - i + 1);

            if (!*extra_data)
                return HTTP_INTERNAL_SERVER_ERROR;

            *extra_data_length = bytes_received - i;
            memcpy(*extra_data, buffer + i, bytes_received - i);
            break;
        }

        i++;

        if (i >= bytes_received)
        {
            bytes_received = recv(client_sockfd, buffer, sizeof(buffer), 0);

            if (bytes_received < 0)
                return HTTP_BAD_REQUEST;

            i = 0;
        }
    }

    if (state != HTTP_STATUS_LINE_DONE)
        return HTTP_BAD_REQUEST;

    if (method_length == 0 || uri_length == 0 || version_length == 0)
        return HTTP_BAD_REQUEST;

    lstring_t method_l = lstring_create(method, method_length);

    if (!method_l)
        return HTTP_INTERNAL_SERVER_ERROR;

    lstring_t uri_l = lstring_create(uri, uri_length);

    if (!uri_l)
    {
        lstring_destroy(method_l);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    request->method = method_l;
    request->uri = uri_l;
    request->query = NULL;
    request->version = strncmp(version, "HTTP/1.0", 8) == 0 ? HTTP_VERSION_10 : 
                         strncmp(version, "HTTP/1.1", 8) == 0 ? HTTP_VERSION_11 : 
                         strncmp(version, "HTTP/0.9", 8) == 0 ? HTTP_VERSION_09 : 
                         HTTP_VERSION_UNKNOWN;

    return HTTP_OK;
}

enum http_response_code http11_parse_request(struct fhttpd_server *server, int client_sockfd, struct http_request *request)
{
    assert(server != NULL && client_sockfd >= 0);

    enum http_response_code ret;
    char *extra_data = NULL;
    size_t extra_data_length = 0;

    if (!request)
        return HTTP_INTERNAL_SERVER_ERROR;

    if ((ret = http11_parse_status_line(server, client_sockfd, request, &extra_data, &extra_data_length)) != HTTP_OK)
        return ret;

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = 0, i = 0;

    enum http_parse_state state = HTTP_PARSE_HEADER_NAME;

    if (extra_data && extra_data_length)
    {
        if (extra_data_length > BUFFER_SIZE)
        {
            free(extra_data);
            return HTTP_BAD_REQUEST;
        }

        memcpy(buffer, extra_data, extra_data_length);
        bytes_received = extra_data_length;
        free(extra_data);
    }

    struct buffer *buffered_body = NULL;

    struct http_header *headers = calloc(16, sizeof(struct http_header));

    if (!headers)
        return HTTP_INTERNAL_SERVER_ERROR;

    size_t header_count = 0, header_capacity = 16;

    while (i < bytes_received)
    {
        switch (state) {
            case HTTP_PARSE_HEADER_NAME:
                if (buffer[i] == ':')
                {
                    state = HTTP_PARSE_HEADER_VALUE;
                    goto continue_loop;
                }

                if (header_count >= HTTP_HEADER_MAX_COUNT)
                {
                    free(headers);
                    return HTTP_BAD_REQUEST;
                }

                if (header_count >= header_capacity)
                {
                    header_capacity += 16;
                    headers = realloc(headers, header_capacity * sizeof(struct http_header));

                    if (!headers)
                        return HTTP_INTERNAL_SERVER_ERROR;

                    for (size_t j = header_count; j < header_capacity; j++)
                        headers[j].name = NULL;
                }

                if (!headers[header_count].name) {
                    headers[header_count].name = lstring_create("", 0);

                    if (!headers[header_count].name)
                        return HTTP_INTERNAL_SERVER_ERROR;
                }

                char c = tolower(buffer[i]);

                if (!lstring_append(&headers[header_count].name, &c, 1))
                {
                    lstring_destroy(headers[header_count].name);
                    return HTTP_INTERNAL_SERVER_ERROR;
                }

                break;

            case HTTP_PARSE_HEADER_VALUE:
                if (buffer[i] == '\r' || buffer[i] == '\n')
                {
                    if (i + 3 < bytes_received && buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r' && buffer[i + 3] == '\n')
                    {
                        state = HTTP_PARSE_DONE;
                        i += 2;
                    }
                    else if (buffer[i] == '\r' && i + 1 < bytes_received && buffer[i + 1] == '\n')
                    {  
                        state = HTTP_PARSE_HEADER_NAME;
                        i++;
                    }

                    header_count++;
                    goto continue_loop;
                }

                if (!headers[header_count].value) {
                    headers[header_count].value = lstring_create("", 0);

                    if (!headers[header_count].value)
                        return HTTP_INTERNAL_SERVER_ERROR;
                }

                if (!lstring_append(&headers[header_count].value, buffer + i, 1))
                {
                    lstring_destroy(headers[header_count].value);
                    return HTTP_INTERNAL_SERVER_ERROR;
                }

                break;

            default:
                assert(false && "Invalid state");
                free(headers);
                return HTTP_INTERNAL_SERVER_ERROR;
        }

    continue_loop:
        i++;

        if (state == HTTP_PARSE_DONE) 
        {
            if (i < bytes_received)
            {
                buffered_body = buffer_create(bytes_received - i);
                buffer_append(buffered_body, (uint8_t *) (buffer + i), bytes_received - i);
            }

            break;
        }

        if (i >= bytes_received)
        {
            bytes_received = recv(client_sockfd, buffer, sizeof(buffer), 0);

            if (bytes_received < 0)
                return HTTP_BAD_REQUEST;

            if (bytes_received == 0)
                break;

            i = 0;
        }
    }

    if (state != HTTP_PARSE_DONE)
    {
        free(headers);
        return HTTP_BAD_REQUEST;
    }

    for (size_t j = 0; j < header_count; j++)
    {
        if (!lstring_trim(&headers[j].name) || !lstring_trim(&headers[j].value))
        {
            lstring_destroy(headers[j].name);
            lstring_destroy(headers[j].value);
            free(headers);
            return HTTP_BAD_REQUEST;
        }
    }

    request->headers = headers;
    request->header_count = header_count;
    request->buffered_body = buffered_body;

    return HTTP_OK;
}

bool http11_send_response(struct fhttpd_server *server, int client_sockfd, struct http_response *response)
{
    assert(server != NULL && client_sockfd >= 0 && response != NULL);
    
    const char *status_text = http_status_code_to_text(response->code);
    const char *version_str = http_version_string(response->version);

    if (!status_text || !version_str)
        return false;

    char status_str[16] = {0};
    snprintf(status_str, sizeof(status_str), "%d", response->code);

    send(client_sockfd, version_str, strlen(version_str), 0);
    send(client_sockfd, " ", 1, 0);
    send(client_sockfd, status_str, strlen(status_str), 0);
    send(client_sockfd, " ", 1, 0);
    send(client_sockfd, status_text, strlen(status_text), 0);
    send(client_sockfd, "\r\n", 2, 0);

    for (size_t i = 0; i < response->header_count; i++)
    {
        send(client_sockfd, response->headers[i].name, lstring_length(response->headers[i].name), 0);
        send(client_sockfd, ": ", 2, 0);
        send(client_sockfd, response->headers[i].value, lstring_length(response->headers[i].value), 0);
        send(client_sockfd, "\r\n", 2, 0);
    }

    if (response->body && response->body->length > 0)
    {
        send(client_sockfd, "\r\n", 2, 0);
        send(client_sockfd, response->body->data, response->body->length, 0);
    }

    return true;
}