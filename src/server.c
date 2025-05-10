#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "server.h"
#include "protocol.h"
#include "http11.h"

enum fhttpd_log_level
{
    FHTTPD_LOG_ERROR,
    FHTTPD_LOG_WARNING,
    FHTTPD_LOG_INFO,
    FHTTPD_LOG_DEBUG
};

struct fhttpd_server
{
    void *fhttpd_config[__FHTTPD_CONFIG_COUNT];
    int sockfd;
    struct sockaddr_in *server_addr;
};

struct fhttpd_server *fhttpd_server_create()
{
    struct fhttpd_server *server = calloc(1, sizeof(struct fhttpd_server));

    if (!server)
        return NULL;

    return server;
}

void fhttpd_server_destroy(struct fhttpd_server *server)
{
    if (!server)
        return;

    free(server->server_addr);
    close(server->sockfd);
    free(server);
}

bool fhttpd_server_set_config(struct fhttpd_server *server, enum fhttpd_server_config config, void *value)
{
    if (!server || config >= __FHTTPD_CONFIG_COUNT)
        return false;

    server->fhttpd_config[config] = value;
    return true;
}

void *fhttpd_server_get_config(struct fhttpd_server *server, enum fhttpd_server_config config)
{
    if (!server || config >= __FHTTPD_CONFIG_COUNT)
        return NULL;

    return server->fhttpd_config[config];
}

static int fhttpd_server_create_socket(void)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
        return -1;

    int opt = 1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        return -1;

    struct timeval timeout;

    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof (timeout)) < 0)
        return -1; 
 
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *) &timeout, sizeof (timeout)) < 0)
        return -1;

    int flags = fcntl(sockfd, F_GETFL, 0);

    if (flags < 0)
        return -1;
    
    if (fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK) < 0)
        return -1;

    return sockfd;
}

errno_t fhttpd_server_initialize(struct fhttpd_server *server)
{
    if (!server)
        return -EINVAL;

    server->sockfd = fhttpd_server_create_socket();

    if (server->sockfd < 0)
        return errno;

    uint16_t port = server->fhttpd_config[FHTTPD_CONFIG_PORT] ? *(uint16_t *)server->fhttpd_config[FHTTPD_CONFIG_PORT] : 80;
    uint32_t bind_addr = server->fhttpd_config[FHTTPD_CONFIG_BIND_ADDR] ? *(uint32_t *)server->fhttpd_config[FHTTPD_CONFIG_BIND_ADDR] : INADDR_ANY;

    if (port == 0)
        return -EINVAL;

    server->server_addr = malloc(sizeof(struct sockaddr_in));

    if (!server->server_addr)
        return errno;

    server->server_addr->sin_family = AF_INET;
    server->server_addr->sin_addr.s_addr = bind_addr;
    server->server_addr->sin_port = htons(port);

    if (bind(server->sockfd, (struct sockaddr *)server->server_addr, sizeof(struct sockaddr_in)) < 0)
        return errno;

    if (listen(server->sockfd, 5) < 0)
        return errno;

    return 0;
}

static bool fhttpd_error(struct fhttpd_server *server, int client_sockfd, enum http_response_code code)
{
    if (!server || client_sockfd < 0)
        return false;

    struct http_response response = {0};
    char content_length[32];

    response.version = HTTP_VERSION_11;
    response.code = code;
    response.body = buffer_create(1);

    buffer_aprintf(response.body, "%d %s\n\n----\nfreehttpd/1.0.0-alpha.1 Server at localhost\n", code, http_status_code_to_text(code));
    snprintf(content_length, sizeof(content_length) - 1, "%zu", response.body->length);
    
    http_response_add_header(&response, "Server", "freehttpd");
    http_response_add_header(&response, "Connection", "close");
    http_response_add_header(&response, "Content-Type", "text/plain; charset=\"utf-8\"");
    http_response_add_header(&response, "Content-Length", content_length);

    bool ret = http11_send_response(server, client_sockfd, &response);
    http_response_destroy_inner(&response);
    close(client_sockfd);

    return ret;
}

void fhttpd_log_request(const struct http_request *request)
{
    printf("[incoming] %s %s %s\n", request->method, request->uri, http_version_string(request->version));

    for (size_t i = 0; i < request->header_count; i++)
        printf("[incoming] %s: %s\n", request->headers[i].name, request->headers[i].value);
}

void fhttpd_log(enum fhttpd_log_level level, const char *format, ...)
{
    FILE *fp = level == FHTTPD_LOG_ERROR || level == FHTTPD_LOG_WARNING ? stderr : stdout;
    va_list args;
    va_start(args, format);
    fprintf(fp, "[fhttpd:%d] ", level);
    vfprintf(fp, format, args);
    va_end(args);
}

errno_t fhttpd_server_start(struct fhttpd_server *server)
{
    if (!server)
        return -1;

    uint16_t accept_errors = 0;

    while (true)
    {
        struct http_request request;
        struct http_response response = {0};
        enum http_response_code rcode;

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sockfd = accept(server->sockfd, (struct sockaddr *) &client_addr, &addr_len);

        if (client_sockfd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                usleep(500);
                continue;
            }

            accept_errors++;

            if (accept_errors >= 1000)
            {
                puts("Too many accept errors, shutting down server.");
                printf("Error: %s\n", strerror(errno));
                return errno;
            }
            
            continue;
        }

        accept_errors = 0;
        rcode = http11_parse_request(server, client_sockfd, &request);

        if (rcode != HTTP_OK)
        {
            fprintf(stderr, "Error parsing request: %d\n", rcode);
            fhttpd_error(server, client_sockfd, rcode);
            http_request_destroy_inner(&request);
            continue;
        }

        fhttpd_log_request(&request);

        if (strcmp(request.method, "GET") != 0 && strcmp(request.method, "HEAD") != 0)
        {
            fhttpd_log(FHTTPD_LOG_WARNING, "Unsupported method: %s\n", request.method);
            fhttpd_error(server, client_sockfd, HTTP_METHOD_NOT_ALLOWED);
            http_request_destroy_inner(&request);
            continue;
        }

        response.code = HTTP_OK;
        response.version = request.version;

        char text[] = "Hello, World!\n";
        char text_length[32];

        snprintf(text_length, sizeof(text_length), "%zu", sizeof(text) - 1);
        
        http_response_add_header(&response, "Server", "freehttpd");
        http_response_add_header(&response, "Connection", "close");

        if (strcmp(request.method, "HEAD") != 0) 
        {
            http_response_add_header(&response, "Content-Type", "text/plain; charset=\"utf-8\"");
            http_response_add_header(&response, "Content-Length", text_length);
        }

        http11_send_response(server, client_sockfd, &response);
        http_response_destroy_inner(&response);

        if (strcmp(request.method, "HEAD") != 0) 
        {
            send(client_sockfd, "\r\n", 2, 0);
            send(client_sockfd, text, sizeof(text) - 1, 0);
        }

        close(client_sockfd);
        http_request_destroy_inner(&request);
    }

    return 0;
}