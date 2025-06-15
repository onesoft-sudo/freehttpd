#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "htable.h"
#include "log.h"

#include "loop.h"
#include "protocol.h"
#include "server.h"
#include "types.h"
#include "utils.h"
#include "connection.h"
#include "htable.h"

#define FHTTPD_DEFAULT_BACKLOG SOMAXCONN
#define MAX_EVENTS 64

static struct fhttpd_server *local_server = NULL;

static struct fhttpd_server *
fhttpd_server_create (const struct fhttpd_master *master)
{
    struct fhttpd_server *server = calloc (1, sizeof (struct fhttpd_server));

    if (!server)
        return NULL;

    server->master_pid = master->pid;
    server->pid = getpid ();
    server->epoll_fd = epoll_create1 (0);
    server->listen_fds = NULL;

    if (server->epoll_fd < 0)
    {
        free (server);
        return NULL;
    }

    server->connections = htable_create (0);

    if (!server->connections)
    {
        close (server->epoll_fd);
        free (server);
        return NULL;
    }

    memcpy (server->config, master->config, sizeof (server->config));
    return server;
}

void *
fhttpd_get_config (struct fhttpd_master *master,
                                 enum fhttpd_config config)
{
    if (config < 0 || config >= FHTTPD_CONFIG_MAX)
        return NULL;

    return master->config[config];
}

void
fhttpd_set_config (struct fhttpd_master *master, enum fhttpd_config config, void *value_ptr)
{
    if (config < 0 || config >= FHTTPD_CONFIG_MAX)
        return;

    master->config[config] = value_ptr;
}

static bool
fhttpd_fd_set_nonblocking (int fd)
{
    int flags = fcntl (fd, F_GETFL);

    if (flags < 0)
        return false;

    if (fcntl (fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return false;

    return true;
}

static bool
fhttpd_epoll_ctl_add (struct fhttpd_server *server, fd_t fd, uint32_t events)
{
    struct epoll_event ev = { 0 };

    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl (server->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
        return false;

    return true;
}
static bool
fhttpd_epoll_ctl_mod (struct fhttpd_server *server, fd_t fd, uint32_t events)
{
    struct epoll_event ev = { 0 };

    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl (server->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
        return false;

    return true;
}

static bool
fhttpd_server_create_sockets (struct fhttpd_server *server)
{
    uint16_t *ports = (uint16_t *) server->config[FHTTPD_CONFIG_PORTS];

    while (*ports)
    {
        int port = *ports;
        int sockfd = socket (AF_INET, SOCK_STREAM, 0);

        if (sockfd < 0)
            return false;

        int opt = 1;
        struct timeval tv = {0};

        tv.tv_sec = 10;

        setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &opt,
                    sizeof (opt));
        setsockopt (sockfd, SOL_SOCKET, SO_REUSEPORT, &opt,
                    sizeof (opt));
        setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv,
                    sizeof (tv));
        setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv,
                    sizeof (tv));

        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons (port),
            .sin_addr.s_addr = INADDR_ANY,
        };

        if (bind (sockfd, (struct sockaddr *) &addr, sizeof (addr)) < 0)
        {
            close (sockfd);
            return false;
        }

        if (listen (sockfd, FHTTPD_DEFAULT_BACKLOG) < 0)
        {
            close (sockfd);
            return false;
        }

        fhttpd_fd_set_nonblocking (sockfd);
        server->listen_fds = realloc (server->listen_fds, sizeof (fd_t) * ++server->listen_fd_count);

        if (!server->listen_fds)
        {
            close (sockfd);
            return false;
        }

        server->listen_fds[server->listen_fd_count - 1] = sockfd;

        if (!fhttpd_epoll_ctl_add (server, sockfd, EPOLLIN))
        {
            close (sockfd);
            return false;
        }

        ports++;
    }

    return true;
}

static bool
fhttpd_server_prepare (struct fhttpd_server *server)
{
    return fhttpd_server_create_sockets (server);
}

void
fhttpd_server_destroy (struct fhttpd_server *server)
{
    if (!server)
        return;

    if (server->listen_fds)
    {
        for (size_t i = 0; i < server->listen_fd_count; i++)
        {
            close (server->listen_fds[i]);
        }

        free (server->listen_fds);
    }

    if (server->epoll_fd >= 0)
        close (server->epoll_fd);

    if (server->connections)
    {
        struct htable_entry *entry = server->connections->head;

        while (entry)
        {
            struct fhttpd_connection *conn = entry->data;

            if (conn)
                fhttpd_connection_free (conn);

            entry = entry->next;
        }

        htable_destroy (server->connections);
    }

    free (server);
}

static struct fhttpd_connection *
fhttpd_server_new_connection (struct fhttpd_server *server, fd_t client_sockfd)
{
    struct fhttpd_connection *conn = fhttpd_connection_create (server->last_connection_id + 1, client_sockfd);

    if (!conn)
        return NULL;

    if (!htable_set (server->connections, client_sockfd, conn))
    {
        fhttpd_connection_free (conn);
        return NULL;
    }

    server->last_connection_id++;
    return conn;
}

static bool
fhttpd_server_free_connection (struct fhttpd_server *server, struct fhttpd_connection *conn)
{
    fd_t fd = conn->client_sockfd;

    if (htable_remove (server->connections, fd) != conn)
        return false;

    fhttpd_wclog_debug ("Connection #%lu is being deallocated", conn->id);
    fhttpd_connection_free (conn);

    if (epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
        return false;

    close (fd);
    return true;
}

static bool
fhttpd_server_accept (struct fhttpd_server *server, size_t fd_index)
{
    if (fd_index >= server->listen_fd_count)
    {
        errno = EINVAL;
        return false;
    }

    fd_t sockfd = server->listen_fds[fd_index];
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof (client_addr);

    fd_t client_sockfd = accept (sockfd, (struct sockaddr *) &client_addr, &addrlen);

    if (client_sockfd < 0)
        return errno == EAGAIN || errno == EWOULDBLOCK;

    char ip[INET_ADDRSTRLEN];
    inet_ntop (AF_INET, &client_addr.sin_addr, ip, sizeof (ip));
    fhttpd_wclog_info ("Accepted connection from %s:%d", ip, ntohs (client_addr.sin_port));

    if (!fhttpd_fd_set_nonblocking (client_sockfd))
    {
        close (client_sockfd);
        return false;
    }

    if (!fhttpd_epoll_ctl_add (server, client_sockfd, EPOLLIN | EPOLLET | EPOLLHUP))
    {
        close (client_sockfd);
        return false;
    }

    struct fhttpd_connection *conn = fhttpd_server_new_connection (server, client_sockfd);

    if (!conn)
    {
        epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, client_sockfd, NULL);
        close (client_sockfd);
        return false;
    }

    fhttpd_wclog_info ("Connection established with %s:%d [ID #%lu]", ip, ntohs (client_addr.sin_port), conn->id);
    return true;
}

static loop_op_t
fhttpd_server_on_read_ready (struct fhttpd_server *server, fd_t client_sockfd)
{
    struct fhttpd_connection *conn = htable_get (server->connections, client_sockfd);

    if (!conn)
    {
        fhttpd_wclog_error ("Client data received, no connection found for socket: %d", client_sockfd);
        epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, client_sockfd, NULL);
        close (client_sockfd);
        return LOOP_OPERATION_NONE;
    }

    if (conn->protocol == FHTTPD_PROTOCOL_UNKNOWN)
    {
        if (!fhttpd_connection_detect_protocol (conn))
        {
            fhttpd_wclog_error ("Connection #%lu: Unable to detect protocol: %s", conn->id, strerror (errno));
            fhttpd_server_free_connection (server, conn);
            return LOOP_OPERATION_NONE;
        }

        if (conn->protocol != FHTTPD_PROTOCOL_UNKNOWN)
            fhttpd_wclog_info ("Connection #%lu: Protocol detected: %s", conn->id, fhttpd_protocol_to_string (conn->protocol));
        else
            return LOOP_OPERATION_NONE;

        switch (conn->protocol)
        {
            case FHTTPD_PROTOCOL_HTTP1x:
                http1_parser_ctx_init (&conn->http1_req_ctx);
                static_assert (sizeof (conn->http1_req_ctx.buffer) >= H2_PREFACE_SIZE);
                memcpy (conn->http1_req_ctx.buffer, conn->buffers.protobuf, H2_PREFACE_SIZE);
                conn->http1_req_ctx.buffer_len = H2_PREFACE_SIZE;
                fhttpd_wclog_info ("Connection #%lu: HTTP/1.x protocol initialized", conn->id);
                break;

            default:
                fhttpd_wclog_error ("Connection #%lu: Unsupported protocol: %d", conn->id, conn->protocol);
                fhttpd_server_free_connection (server, conn);
                return LOOP_OPERATION_NONE;
        }
    }

    switch (conn->protocol)
    {
        case FHTTPD_PROTOCOL_HTTP1x:
            if (!http1_parse (conn, &conn->http1_req_ctx))
            {
                fhttpd_wclog_error ("Connection #%lu: HTTP/1.x parsing failed", conn->id);

                if (!fhttpd_connection_defer_error_response (conn, 0, FHTTPD_STATUS_BAD_REQUEST))
                    fhttpd_server_free_connection (server, conn);

                return LOOP_OPERATION_NONE;
            }

            if (conn->http1_req_ctx.state == HTTP1_STATE_DONE)
            {
                fhttpd_wclog_info ("Connection #%lu: HTTP/1.x request parsed successfully", conn->id);

                if (!fhttpd_epoll_ctl_mod (server, conn->client_sockfd, EPOLLOUT | EPOLLHUP))
                {
                    fhttpd_wclog_error ("Connection #%lu: Failed to modify epoll events: %s", conn->id, strerror (errno));
                    fhttpd_server_free_connection (server, conn);
                    return LOOP_OPERATION_NONE;
                }

                struct fhttpd_request *requests = realloc (conn->requests, sizeof (struct fhttpd_request) * (conn->request_count + 1));

                if (!requests)
                {
                    fhttpd_wclog_error("Memory allocation failed");
                    
                    if (!fhttpd_connection_defer_error_response (conn, 0, FHTTPD_STATUS_INTERNAL_SERVER_ERROR))
                        fhttpd_server_free_connection (server, conn);

                    return LOOP_OPERATION_NONE;
                }

                conn->requests = requests;

                struct fhttpd_request *request = &conn->requests[conn->request_count++];

                request->protocol = conn->protocol;
                request->method = conn->http1_req_ctx.result.method;
                request->uri = conn->http1_req_ctx.result.uri;
                request->uri_len = conn->http1_req_ctx.result.uri_len;
                request->headers = conn->http1_req_ctx.result.headers;
                request->body = (uint8_t *) conn->http1_req_ctx.result.body;
                request->body_len = conn->http1_req_ctx.result.body_len;

                memcpy(conn->exact_protocol, conn->http1_req_ctx.result.version, 4);
                conn->http1_req_ctx.result.used = true;
                fhttpd_wclog_info ("Connection #%lu: Accepted request #%zu", conn->id, conn->request_count - 1);
                return LOOP_OPERATION_NONE;
            }
            else if (conn->http1_req_ctx.state == HTTP1_STATE_RECV)
            {
                fhttpd_wclog_debug ("Connection #%lu: Waiting for more data in HTTP/1.x parser", conn->id);
                return LOOP_OPERATION_NONE;
            }
            else if (conn->http1_req_ctx.state == HTTP1_STATE_ERROR)
            {
                fhttpd_wclog_debug ("Connection #%lu: HTTP/1.x parser encountered an error", conn->id);

                if (!fhttpd_epoll_ctl_mod (server, conn->client_sockfd, EPOLLOUT | EPOLLHUP))
                {
                    fhttpd_wclog_error ("Connection #%lu: Failed to modify epoll events: %s", conn->id, strerror (errno));
                    fhttpd_server_free_connection (server, conn);
                    return LOOP_OPERATION_NONE;
                }

                if (!fhttpd_connection_defer_error_response (conn, 0, FHTTPD_STATUS_BAD_REQUEST))
                    fhttpd_server_free_connection (server, conn);

                return LOOP_OPERATION_NONE;
            }
            break;

        default:
            fhttpd_wclog_error ("Connection #%lu: Unsupported protocol: %d", conn->id, conn->protocol);
            fhttpd_server_free_connection (server, conn);
            return LOOP_OPERATION_NONE;
    }
    
    return LOOP_OPERATION_NONE;
}

static loop_op_t
fhttpd_server_on_write_ready (struct fhttpd_server *server, fd_t client_sockfd)
{
    struct fhttpd_connection *conn = htable_get (server->connections, client_sockfd);

    if (!conn)
    {
        fhttpd_wclog_error ("No connection found for socket: %d", client_sockfd);
        epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, client_sockfd, NULL);
        close (client_sockfd);
        return LOOP_OPERATION_NONE;
    }

    if (conn->protocol == FHTTPD_PROTOCOL_UNKNOWN)
    {
        fhttpd_wclog_error ("No known protocol for connection #%lu", conn->id);
        fhttpd_server_free_connection (server, conn);        
        return LOOP_OPERATION_NONE;
    }

    if (conn->request_count > conn->response_count)
    {
        fhttpd_wclog_debug ("Connection #%lu: Some requests need to be processed", conn->id);

        for (size_t i = 0; i < conn->request_count; i++)
        {
            if (!fhttpd_connection_defer_error_response (conn, i, FHTTPD_STATUS_NOT_FOUND))
            {
                fhttpd_wclog_debug ("Connection #%lu: Failed to defer response", conn->id);
                fhttpd_server_free_connection (server, conn);
                return LOOP_OPERATION_NONE;
            }
        }
    }

    if (conn->response_count == 0)
    {
        fhttpd_wclog_debug ("Connection #%lu: No responses to send", conn->id);
        fhttpd_server_free_connection (server, conn);
        return LOOP_OPERATION_NONE;
    }

    for (size_t i = 0; i < conn->response_count; i++)
    {
        struct fhttpd_response *response = &conn->responses[i];

        if (response->sent)
            continue;

        if (!fhttpd_connection_send_response (conn, i, response))
        {
            fhttpd_wclog_debug ("Connection #%lu: Failed to send response", conn->id);
            fhttpd_server_free_connection (server, conn);
            return LOOP_OPERATION_NONE;
        }

        if (would_block())
        {
            fhttpd_wclog_debug ("Connection #%lu: Client cannot accept data right now, waiting for next event", conn->id);
            return LOOP_OPERATION_NONE;
        }

        response->sent = true;
    }

    fhttpd_wclog_info ("Connection #%lu: Response sent successfully", conn->id);
    fhttpd_server_free_connection (server, conn);
    return LOOP_OPERATION_NONE;
}

static loop_op_t
fhttpd_server_on_hup (struct fhttpd_server *server, fd_t client_sockfd)
{
    struct fhttpd_connection *conn = htable_get (server->connections, client_sockfd);

    if (!conn)
    {
        fhttpd_wclog_error ("Client hangup received, no connection found for socket: %d", client_sockfd);
        epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, client_sockfd, NULL);
        close (client_sockfd);
        return LOOP_OPERATION_NONE;
    }

    fhttpd_wclog_info ("Client hangup received in connection: %lu", conn->id);
    fhttpd_server_free_connection (server, conn);

    return LOOP_OPERATION_NONE;
}

void
fhttpd_server_loop (struct fhttpd_server *server)
{
    struct epoll_event events[MAX_EVENTS];

    while (true)
    {
        int nfds = epoll_wait (server->epoll_fd, events, MAX_EVENTS, -1);

        if (nfds < 0)
        {
            fhttpd_wclog_error ("epoll_wait() returned %d", nfds);
            exit (EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; i++)
        {
            bool is_listen_fd = false;

            for (size_t j = 0; j < server->listen_fd_count; j++)
            {
                if (server->listen_fds[j] == events[i].data.fd)
                {
                    if (!fhttpd_server_accept (server, j))
                        fhttpd_wclog_error ("Error accepting new connection: %s", strerror (errno));

                    is_listen_fd = true;
                    break;
                }
            }

            if (is_listen_fd)
                continue;

            uint32_t ev = events[i].events;

            if (ev & EPOLLIN)
            {
                loop_op_t op = fhttpd_server_on_read_ready (server, events[i].data.fd);
                LOOP_OPERATION (op);
            }
            else if (ev & EPOLLOUT)
            {
                loop_op_t op = fhttpd_server_on_write_ready (server, events[i].data.fd);
                LOOP_OPERATION (op);
            }

            if (ev & EPOLLHUP)
            {
                loop_op_t op = fhttpd_server_on_hup (server, events[i].data.fd);
                LOOP_OPERATION (op);
            }
        }
    }
}

static void
fhttpd_worker_exit_handler (void)
{
    if (local_server)
        fhttpd_server_destroy (local_server);
}

static void
fhttpd_print_info (const struct fhttpd_master *master)
{
    uint16_t *ports = (uint16_t *) master->config[FHTTPD_CONFIG_PORTS];

    while (*ports)
    {
        fhttpd_log_info ("Listening on port %d", *ports);
        ports++;
    }
}

bool fhttpd_master_start (struct fhttpd_master *master)
{
    size_t worker_count = *(size_t *) master->config[FHTTPD_CONFIG_WORKER_COUNT];

    master->workers = calloc (worker_count, sizeof (pid_t));

    if (!master->workers)
        return false;

    master->worker_count = worker_count;

    for (size_t i = 0; i < worker_count; i++)
    {
        pid_t pid = fork();

        if (pid < 0)
            return false;

        if (pid == 0)
        {
            atexit (&fhttpd_worker_exit_handler);

            struct fhttpd_server *server = fhttpd_server_create (master);

            if (!server)
            {
                fhttpd_wclog_error ("Failed to create server: %s\n", strerror (errno));
                exit (EXIT_FAILURE);
            }

            local_server = server;

            if (!fhttpd_server_prepare (server))
            {
                fhttpd_wclog_error ("Failed to prepare server: %s\n", strerror (errno));
                exit (EXIT_FAILURE);
            }

            fhttpd_server_loop (server);
            fhttpd_server_destroy (server);

            local_server = NULL;
            exit (EXIT_FAILURE);
        }
        else
        {
            master->workers[i] = pid;
            fhttpd_log_info ("Started worker process: %d", pid);
        }
    }

    fhttpd_print_info (master);

    for (size_t i = 0; i < worker_count; i++)
        waitpid(master->workers[i], NULL, 0);

    return false;
}

void fhttpd_master_destroy (struct fhttpd_master *master)
{
    if (!master)
        return;

    if (master->pid == getpid ())
    {
        for (size_t i = 0; i < master->worker_count; i++)
        {
            kill (master->workers[i], SIGTERM);
        }
    }

    free (master->workers);
    free (master);
}

struct fhttpd_master *fhttpd_master_create (void)
{
    struct fhttpd_master *master = calloc (1, sizeof (struct fhttpd_master));

    if (!master)
        return NULL;

    master->pid = getpid();
    return master;
}
