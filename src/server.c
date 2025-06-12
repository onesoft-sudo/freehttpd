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
#include <time.h>
#include <unistd.h>

#include "htable.h"
#include "http11.h"
#include "log.h"
#include "loop.h"
#include "protocol.h"
#include "server.h"
#include "utils.h"

#define FHTTPD_DEFAULT_BACKLOG SOMAXCONN
#define MAX_EVENTS 64

struct fhttpd_worker
{
    struct fhttpd_server *server;

    pid_t pid;
    int sockfd;
    int epoll_fd;

    /* Maps client sockets to struct fhttpd_connection * */
    struct htable *connections;
};

struct fhttpd_server
{
    void *config[FHTTPD_CONFIG_MAX];

    int *sockets;
    struct fhttpd_worker *workers;
    size_t num_sockets;

    pid_t master_pid;
    uint64_t last_connection_id;
};

struct fhttpd_server *
fhttpd_server_create (void)
{
    struct fhttpd_server *server
        = mmap (NULL, sizeof (struct fhttpd_server), PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (!server)
        return NULL;

    bzero (server, sizeof (struct fhttpd_server));

    server->master_pid = getpid ();
    server->last_connection_id = 0;

    return server;
}

void
fhttpd_server_set_config (struct fhttpd_server *server,
                          enum fhttpd_config config, void *value)
{
    if (!server || config < 0 || config >= FHTTPD_CONFIG_MAX)
        return;

    server->config[config] = value;
}

void *
fhttpd_server_get_config (struct fhttpd_server *server,
                          enum fhttpd_config config)
{
    if (config < 0 || config >= FHTTPD_CONFIG_MAX)
        return NULL;

    return server->config[config];
}

pid_t
fhttpd_server_get_master_pid (struct fhttpd_server *server)
{
    if (!server)
        return -1;

    return server->master_pid;
}

void
fhttpd_server_destroy (struct fhttpd_server *server)
{
    if (!server)
        return;

    if (server->sockets)
    {
        for (size_t i = 0; i < server->num_sockets; i++)
        {
            if (server->sockets[i] >= 2)
                close (server->sockets[i]);
        }

        free (server->sockets);
    }

    pid_t pid = getpid ();

    if (pid != server->master_pid)
    {
        if (server->workers)
        {
            for (size_t i = 0; i < server->num_sockets; i++)
            {
                if (server->workers[i].pid == pid)
                {
                    close (server->workers[i].epoll_fd);
                    htable_destroy (server->workers[i].connections);
                }
            }
        }

        return;
    }

    if (server->workers)
    {
        for (size_t i = 0; i < server->num_sockets; i++)
        {
            if (server->workers[i].pid > 0)
            {
                fhttpd_log_warning ("Terminating worker process: %d",
                                    server->workers[i].pid);
                kill (server->workers[i].pid, SIGTERM);
            }
        }

        munmap (server->workers,
                sizeof (struct fhttpd_worker) * server->num_sockets);
    }

    munmap (server, sizeof (struct fhttpd_server));
}

static int
fhttpd_epoll_ctl_add (int epoll_fd, int sockfd, uint32_t events)
{
    struct epoll_event ev = { 0 };

    ev.events = events;
    ev.data.fd = sockfd;

    if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) < 0)
    {
        int err = -errno;
        fhttpd_wclog_perror ("epoll_ctl");
        return err;
    }

    return 0;
}

static int
fhttpd_socket_set_non_blocking (int sockfd)
{
    int flags = fcntl (sockfd, F_GETFL, 0);

    if (flags < 0)
        return -errno;

    if (fcntl (sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -errno;

    return 0;
}

static int
fhttpd_socket_create ()
{
    int sockfd = socket (AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
        return ERRNO_GENERIC;

    int opt = 1;

    if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt)) < 0)
    {
        int err = errno;
        close (sockfd);
        return -err;
    }

    if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof (opt)) < 0)
    {
        int err = errno;
        close (sockfd);
        return -err;
    }

    struct timeval timeout = { 0 };

    timeout.tv_sec = 10;

    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof (timeout))
        < 0)
    {
        int err = errno;
        close (sockfd);
        return -err;
    }

    if (setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof (timeout))
        < 0)
    {
        int err = errno;
        close (sockfd);
        return -err;
    }

    if (fhttpd_socket_set_non_blocking (sockfd) < 0)
    {
        int err = errno;
        close (sockfd);
        return -err;
    }

    return sockfd;
}

static int
fhttpd_socket_bind (int sockfd, const char *ip, uint16_t port)
{
    struct sockaddr_in addr = { 0 };

    addr.sin_family = AF_INET;
    addr.sin_port = htons (port);

    if (ip && inet_pton (AF_INET, ip, &addr.sin_addr) <= 0)
        return ERRNO_GENERIC;
    else if (!ip)
        addr.sin_addr.s_addr = INADDR_ANY;

    if (bind (sockfd, (struct sockaddr *) &addr, sizeof (addr)) < 0)
    {
        int err = errno;
        close (sockfd);
        return -err;
    }

    return 0;
}

static int
fhttpd_socket_listen (int sockfd, int backlog)
{
    if (listen (sockfd, backlog == 0 ? FHTTPD_DEFAULT_BACKLOG : backlog) < 0)
    {
        int err = errno;
        close (sockfd);
        return -err;
    }

    return 0;
}

static struct fhttpd_connection *
fhttpd_new_connection (struct fhttpd_worker *worker, int client_sockfd,
                       const struct sockaddr_in *client_addr,
                       socklen_t addr_len)
{
    struct fhttpd_connection *connection
        = calloc (1, sizeof (struct fhttpd_connection));

    if (!connection)
        return NULL;

    connection->id = worker->server->last_connection_id++;
    connection->protocol = FHTTPD_PROTOCOL_UNKNOWN;
    connection->client_sockfd = client_sockfd;
    connection->client_addr = *client_addr;
    connection->addr_len = addr_len;
    connection->requests = NULL;
    connection->num_requests = 0;
    connection->buffer_len = 0;
    connection->created_at_ts = utils_get_current_timestamp ();
    connection->last_recv_activity_ts = connection->created_at_ts;

    if (!htable_set (worker->connections, client_sockfd, connection))
    {
        fhttpd_wclog_error ("Failed to set connection in hashtable");
        free (connection);
        return NULL;
    }

    return connection;
}

static void
fhttpd_free_connection (struct fhttpd_worker *worker,
                        struct fhttpd_connection *connection)
{
    if (!connection)
        return;

    htable_remove (worker->connections, connection->client_sockfd);

    if (connection->requests)
    {
        for (size_t i = 0; i < connection->num_requests; i++)
            free (connection->requests[i].uri);

        free (connection->requests);
    }

    free (connection->http11_parser_ctx);
    free (connection);
}

static loop_operation_t
fhttpd_server_loop_accept (struct fhttpd_worker *worker)
{
    int epoll_fd = worker->epoll_fd;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof (client_addr);
    int client_sockfd
        = accept (worker->sockfd, (struct sockaddr *) &client_addr, &addr_len);

    if (client_sockfd < 0)
    {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            return LOOP_OPERATION_CONTINUE;

        fhttpd_wclog_perror ("accept");
        return LOOP_OPERATION_CONTINUE;
    }

    char client_ip[INET_ADDRSTRLEN];

    if (inet_ntop (AF_INET, &client_addr.sin_addr, client_ip,
                   sizeof (client_ip))
        == NULL)
    {
        fhttpd_wclog_perror ("inet_ntop");
        close (client_sockfd);
        return LOOP_OPERATION_CONTINUE;
    }

    fhttpd_wclog_debug ("Accepted connection from %s:%d", client_ip,
                        ntohs (client_addr.sin_port));

    struct fhttpd_connection *connection
        = fhttpd_new_connection (worker, client_sockfd, &client_addr, addr_len);

    if (!connection)
    {
        fhttpd_wclog_error ("Failed to allocate memory for new connection");
        close (client_sockfd);
        return LOOP_OPERATION_CONTINUE;
    }

    fhttpd_wclog_info ("New connection established: %s:%d", client_ip,
                       ntohs (client_addr.sin_port));

    fhttpd_socket_set_non_blocking (client_sockfd);
    fhttpd_epoll_ctl_add (epoll_fd, client_sockfd,
                          EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);

    return LOOP_OPERATION_NONE;
}

static void
fhttpd_server_close_sockfd (struct fhttpd_worker *worker, int sockfd)
{
    if (sockfd < 0)
        return;

    epoll_ctl (worker->epoll_fd, EPOLL_CTL_DEL, sockfd, NULL);
    close (sockfd);

    struct fhttpd_connection *connection
        = htable_get (worker->connections, sockfd);

    if (connection)
    {
        fhttpd_wclog_info ("Connection closed for socket %d [cleaning up]",
                           sockfd);
        fhttpd_free_connection (worker, connection);
    }
    else
    {
        fhttpd_wclog_error ("Failed to find connection for socket %d", sockfd);
    }
}

ssize_t
fhttpd_connection_recv (struct fhttpd_connection *connection, void *buf,
                        size_t len, int flags)
{
    ssize_t bytes_read = recv (connection->client_sockfd, buf, len, flags);
    int err = errno;

    if (bytes_read < 0)
        return bytes_read;

    if (bytes_read > 0)
    {
        connection->last_recv_activity_ts = utils_get_current_timestamp ();
    }

    errno = err;
    return bytes_read;
}

static loop_operation_t
fhttpd_server_detect_protocol (struct fhttpd_worker *worker,
                               const struct epoll_event *event,
                               struct fhttpd_connection *connection)
{
    ssize_t bytes_read = fhttpd_connection_recv (
        connection, connection->buffer + connection->buffer_len,
        sizeof (connection->buffer) - connection->buffer_len, 0);

    if (bytes_read < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return LOOP_OPERATION_CONTINUE;

        fhttpd_wclog_perror ("fhttpd_connection_recv");
        fhttpd_server_close_sockfd (worker, event->data.fd);
        return LOOP_OPERATION_CONTINUE;
    }
    else if (bytes_read == 0)
    {
        fhttpd_server_close_sockfd (worker, event->data.fd);
        return LOOP_OPERATION_CONTINUE;
    }

    connection->buffer_len += bytes_read;

    if (connection->buffer_len < H2_PREFACE_SIZE)
    {
        fhttpd_wclog_debug ("Received %zd bytes, waiting for more data",
                            bytes_read);
        return LOOP_OPERATION_CONTINUE;
    }

    if (memcmp (connection->buffer, H2_PREFACE, H2_PREFACE_SIZE) == 0)
        connection->protocol = FHTTPD_PROTOCOL_H2;
    else
        connection->protocol = FHTTPD_PROTOCOL_HTTP1x;

    return LOOP_OPERATION_NONE;
}

static loop_operation_t
fhttpd_server_on_read_ready (struct fhttpd_worker *worker,
                             const struct epoll_event *event)
{
    struct fhttpd_connection *connection
        = htable_get (worker->connections, event->data.fd);

    if (!connection)
    {
        fhttpd_wclog_error ("Connection not found for socket %d",
                            event->data.fd);
        fhttpd_server_close_sockfd (worker, event->data.fd);
        return LOOP_OPERATION_CONTINUE;
    }

    if (connection->protocol == FHTTPD_PROTOCOL_UNKNOWN)
    {
        loop_operation_t op
            = fhttpd_server_detect_protocol (worker, event, connection);

        if ((connection->protocol == FHTTPD_PROTOCOL_UNKNOWN
             && (errno == EAGAIN || errno == EWOULDBLOCK))
            || op != LOOP_OPERATION_NONE)
            return op;

        if (connection->protocol == FHTTPD_PROTOCOL_UNKNOWN)
        {
            fhttpd_wclog_error ("Failed to detect protocol for connection %lu",
                                connection->id);
            fhttpd_server_close_sockfd (worker, event->data.fd);
            return LOOP_OPERATION_CONTINUE;
        }

        fhttpd_wclog_info ("Detected protocol: %s",
                           fhttpd_protocol_to_string (connection->protocol));

        struct fhttpd_request *new_requests = realloc (
            connection->requests,
            sizeof (struct fhttpd_request) * (connection->num_requests + 1));

        if (!new_requests)
        {
            fhttpd_wclog_error ("Failed to allocate memory for new request");
            fhttpd_server_close_sockfd (worker, event->data.fd);
            return LOOP_OPERATION_CONTINUE;
        }

        new_requests[connection->num_requests].uri = NULL;
        connection->requests = new_requests;
        connection->num_requests++;

        switch (connection->protocol)
        {
            case FHTTPD_PROTOCOL_HTTP1x:
                {
                    connection->http11_parser_ctx
                        = calloc (1, sizeof (struct http11_parser_ctx));
                    connection->http11_parser_ctx->request
                        = &connection->requests[connection->num_requests - 1];

                    break;
                }

            default:
                fhttpd_wclog_warning ("Unsupported protocol for connection %lu",
                                      connection->id);
                fhttpd_server_close_sockfd (worker, event->data.fd);
                return LOOP_OPERATION_CONTINUE;
        }
    }

    switch (connection->protocol)
    {
        case FHTTPD_PROTOCOL_HTTP1x:
            fhttpd_wclog_info ("Handling HTTP/1.x protocol for connection %lu",
                               connection->id);
            struct http11_parser_ctx *ctx = connection->http11_parser_ctx;

            enum http11_parse_error error;

            while (true)
            {
                error = http11_stream_parse_request (connection, ctx);

                if (error == HTTP11_PARSE_ERROR_WAIT)
                {
                    fhttpd_wclog_debug ("HTTP/1.x request not complete, "
                                        "waiting for more data");
                    break;
                }

                if (error != HTTP11_PARSE_ERROR_NONE
                    || ctx->state == HTTP11_PARSE_STATE_ERROR)
                {
                    fhttpd_wclog_error (
                        "Error parsing HTTP/1.x request: %d [state: %d]", error,
                        ctx->state);
                    fhttpd_server_close_sockfd (worker, event->data.fd);
                    return LOOP_OPERATION_CONTINUE;
                }

                if (ctx->state == HTTP11_PARSE_STATE_COMPLETE)
                    break;

                fhttpd_wclog_debug ("Parsing HTTP/1.x request...");
            }

            if (ctx->state == HTTP11_PARSE_STATE_COMPLETE)
            {
                fhttpd_wclog_debug ("Finished parsing HTTP/1.x request");
                fhttpd_wclog_info (
                    "\033[1;34mIncoming:\033[0m Connection %lu: \033[1m%s "
                    "%s "
                    "%s\033[0m",
                    connection->id,
                    fhttpd_method_to_string (ctx->request->method),
                    ctx->request->uri,
                    fhttpd_protocol_to_string (ctx->request->protocol));

                struct epoll_event new_event
                    = { .events = EPOLLOUT | EPOLLRDHUP | EPOLLHUP,
                        .data = event->data };

                epoll_ctl (worker->epoll_fd, EPOLL_CTL_MOD, event->data.fd,
                           &new_event);
            }

            break;

        default:
            fhttpd_wclog_warning ("Unsupported protocol for connection %lu",
                                  connection->id);
            fhttpd_server_close_sockfd (worker, event->data.fd);
            return LOOP_OPERATION_CONTINUE;
    }

    return LOOP_OPERATION_NONE;
}

static loop_operation_t
fhttpd_server_on_write_ready (struct fhttpd_worker *worker,
                              const struct epoll_event *event)
{
    struct fhttpd_connection *connection
        = htable_get (worker->connections, event->data.fd);

    if (!connection)
    {
        fhttpd_wclog_error ("Connection not found for socket %d",
                            event->data.fd);
        fhttpd_server_close_sockfd (worker, event->data.fd);
        return LOOP_OPERATION_CONTINUE;
    }

    switch (connection->protocol)
    {
        case FHTTPD_PROTOCOL_HTTP1x:
            fhttpd_wclog_info (
                "Handling HTTP/1.x protocol for write-ready socket %d",
                event->data.fd);

            struct http11_parser_ctx *ctx = connection->http11_parser_ctx;

            if (ctx->state != HTTP11_PARSE_STATE_COMPLETE)
            {
                fhttpd_wclog_warning (
                    "HTTP/1.x request not complete, cannot send response");
                return LOOP_OPERATION_CONTINUE;
            }

            int rc = dprintf (
                connection->client_sockfd,
                "HTTP/1.1 200 OK\r\nServer: freehttpd\r\nConnection: "
                "Close\r\nX-FHTTPD-Message: Thank you for using "
                "freehttpd!\r\nContent-Type: text/plain; "
                "charset=UTF-8\r\nContent-Length: "
                "%zu\r\n\r\nFile: %s\n",
                7 + ctx->request->uri_length, ctx->request->uri);

            if (rc < 0)
            {
                fhttpd_wclog_error ("Error sending response (200 OK) - %s",
                                    strerror (errno));
            }

            break;

        default:
            fhttpd_wclog_warning ("Unsupported protocol connection %lu",
                                  connection->id);
            fhttpd_server_close_sockfd (worker, event->data.fd);
            return LOOP_OPERATION_CONTINUE;
    }

    fhttpd_server_close_sockfd (worker, event->data.fd);
    return LOOP_OPERATION_CONTINUE;
}

static loop_operation_t
fhttpd_server_on_hup (struct fhttpd_worker *worker,
                      const struct epoll_event *event)
{
    fhttpd_wclog_info ("Socket %d HUP: closed by peer", event->data.fd);
    epoll_ctl (worker->epoll_fd, EPOLL_CTL_DEL, event->data.fd, NULL);
    close (event->data.fd);
    struct fhttpd_connection *connection
        = htable_get (worker->connections, event->data.fd);

    if (connection)
    {
        fhttpd_wclog_info ("Cleaning up connection %lu", connection->id);
        fhttpd_free_connection (worker, connection);
    }
    else
    {
        fhttpd_wclog_error ("Connection not found for socket %d",
                            event->data.fd);
    }

    return LOOP_OPERATION_CONTINUE;
}

static int
fhttpd_server_enter_loop (struct fhttpd_worker *worker)
{
    struct epoll_event events[MAX_EVENTS];
    uint64_t last_timeout_check = 0;

    while (true)
    {
        int nsockfds = epoll_wait (worker->epoll_fd, events, MAX_EVENTS, 2000);

        if (nsockfds < 0)
        {
            int err = -errno;
            fhttpd_wclog_perror ("epoll_wait");
            return err;
        }

        uint64_t now = utils_get_current_timestamp ();
        loop_operation_t op = LOOP_OPERATION_NONE;

        for (int i = 0; i < nsockfds; i++)
        {
            if (events[i].data.fd == worker->sockfd)
            {
                op = fhttpd_server_loop_accept (worker);
            }
            else if (events[i].events & EPOLLIN)
            {
                op = fhttpd_server_on_read_ready (worker, &events[i]);
            }
            else if (events[i].events & EPOLLOUT)
            {
                op = fhttpd_server_on_write_ready (worker, &events[i]);
            }
            else if (events[i].events & EPOLLERR)
            {
                fhttpd_wclog_error ("Socket %d has an error",
                                    events[i].data.fd);
                fhttpd_server_close_sockfd (worker, events[i].data.fd);
            }

            if (events[i].events & EPOLLHUP)
            {
                loop_operation_t local_op
                    = fhttpd_server_on_hup (worker, &events[i]);

                if (op == LOOP_OPERATION_NONE)
                    op = local_op;
            }
        }

        if (now - last_timeout_check >= 4000)
        {
            last_timeout_check = now;
            uint32_t recv_timeout
                = *(uint32_t *) worker->server
                       ->config[FHTTPD_CONFIG_CLIENT_RECV_TIMEOUT];
            uint32_t header_timeout
                = *(uint32_t *) worker->server
                       ->config[FHTTPD_CONFIG_CLIENT_HEADER_TIMEOUT];

            for (struct htable_entry *entry = worker->connections->head; entry;)
            {
                struct fhttpd_connection *connection = entry->data;
                bool to_close = false;

                if (!connection)
                {
                    entry = entry->next;
                    continue;
                }

                if (connection->protocol == FHTTPD_PROTOCOL_HTTP1x
                    && connection->http11_parser_ctx->state
                           <= HTTP11_PARSE_STATE_HEADERS
                    && connection->created_at_ts + header_timeout < now)
                {
                    fhttpd_wclog_info (
                        "Connection %lu timed out - did not finish sending "
                        "headers in time, closing socket %d",
                        connection->id, connection->client_sockfd);
                    to_close = true;
                }
                else if (connection->last_recv_activity_ts + recv_timeout < now)
                {
                    fhttpd_wclog_info (
                        "Connection %lu timed out, closing socket %d",
                        connection->id, connection->client_sockfd);
                    to_close = true;
                }

                if (to_close)
                {
                    entry = entry->next;
                    fhttpd_server_close_sockfd (worker,
                                                connection->client_sockfd);
                    continue;
                }

                entry = entry->next;
            }
        }

        LOOP_OPERATION (op);
    }

    return 0;
}

static int
fhttpd_server_setup_sockets (struct fhttpd_server *server)
{
    uint16_t *ports = server->config[FHTTPD_CONFIG_PORTS]
                          ? (uint16_t *) server->config[FHTTPD_CONFIG_PORTS]
                          : NULL;

    while (ports && *ports)
    {
        int sockfd = fhttpd_socket_create ();

        if (sockfd < 0)
            return sockfd;

        int ret;

        if ((ret = fhttpd_socket_bind (sockfd, NULL, *ports)) < 0)
        {
            close (sockfd);
            return ret;
        }

        if ((ret = fhttpd_socket_listen (sockfd, FHTTPD_DEFAULT_BACKLOG)) < 0)
        {
            close (sockfd);
            return -ret;
        }

        server->sockets = realloc (server->sockets,
                                   sizeof (int) * (server->num_sockets + 1));

        if (!server->sockets)
        {
            close (sockfd);
            return ERRNO_GENERIC;
        }

        server->sockets[server->num_sockets++] = sockfd;
        ports++;
    }

    if (server->num_sockets == 0)
        return ERRNO_GENERIC;

    return ERRNO_SUCCESS;
}

static void
fhttpd_print_worker_info (const struct fhttpd_worker *worker)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof (addr);

    if (getsockname (worker->sockfd, (struct sockaddr *) &addr, &addr_len) < 0)
    {
        perror ("getsockname");
        return;
    }

    char ip[INET_ADDRSTRLEN];

    if (inet_ntop (AF_INET, &addr.sin_addr, ip, sizeof (ip)) == NULL)
    {
        perror ("inet_ntop");
        return;
    }

    fhttpd_wlog_info (worker->pid, "Listening on socket %d: bound to %s:%d",
                      worker->sockfd, ip, ntohs (addr.sin_port));
}

static int
fhttpd_server_fork_socket_workers (struct fhttpd_server *server)
{
    server->workers
        = mmap (NULL, sizeof (struct fhttpd_worker) * server->num_sockets,
                PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (!server->workers)
        return ERRNO_GENERIC;

    for (size_t i = 0; i < server->num_sockets; i++)
    {
        int sockfd = server->sockets[i];

        pid_t pid = fork ();

        if (pid < 0)
            return -errno;
        else if (pid != 0)
        {
            server->workers[i].pid = pid;
            server->workers[i].sockfd = sockfd;
            server->workers[i].epoll_fd = -1;
            server->workers[i].connections = NULL;
        }
        else
        {
            fhttpd_wclog_debug ("Worker %zu started with PID %d", i, getpid ());

            int epoll_fd = epoll_create (1);
            int ret;

            if (epoll_fd < 0)
            {
                int err = -errno;
                fhttpd_wclog_perror ("epoll_create");
                return err;
            }

            fhttpd_epoll_ctl_add (epoll_fd, sockfd, EPOLLIN | EPOLLRDHUP);

            server->workers[i].epoll_fd = epoll_fd;
            server->workers[i].connections = htable_create (0);
            server->workers[i].server = server;

            if ((ret = fhttpd_server_enter_loop (&server->workers[i])) < 0)
            {
                fhttpd_wclog_error (
                    "Failed to enter server loop for worker %zu: %s", i,
                    strerror (-ret));
            }

            fhttpd_wclog_info ("exiting");
            exit (EXIT_FAILURE);
        }
    }

    return ERRNO_SUCCESS;
}

int
fhttpd_server_run (struct fhttpd_server *server)
{
    if (!server)
        return ERRNO_GENERIC;

    int ret;

    if ((ret = fhttpd_server_setup_sockets (server)) < 0)
        return ret;

    if ((ret = fhttpd_server_fork_socket_workers (server)) < 0)
    {
        fhttpd_log_error ("Failed to fork child processes: %s",
                          strerror (-ret));
        return ret;
    }

    for (size_t i = 0; i < server->num_sockets; i++)
    {
        fhttpd_print_worker_info (&server->workers[i]);
    }

    while (true)
    {
        pause ();
    }

    return ERRNO_SUCCESS;
}