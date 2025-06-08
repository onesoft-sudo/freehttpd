#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"
#include "protocol.h"
#include "server.h"

#define FHTTPD_DEFAULT_BACKLOG 8

struct fhttpd_server
{
    int shmid;
    void *config[__FHTTPD_CONFIG_MAX];
    int *sockets;
    size_t num_sockets;
    pid_t *workers;
    size_t num_workers;
    pid_t master_pid;
};

struct fhttpd_server *
fhttpd_server_create (void)
{
    int shmid
        = shmget (IPC_PRIVATE, sizeof (struct fhttpd_server), IPC_CREAT | 0600);

    if (shmid < 0)
        return NULL;

    struct fhttpd_server *shared_server = shmat (shmid, NULL, 0);

    if (!shared_server)
        return NULL;

    bzero (shared_server, sizeof (struct fhttpd_server));
    shared_server->shmid = shmid;
    shared_server->master_pid = getpid ();

    return shared_server;
}

void
fhttpd_server_set_config (struct fhttpd_server *server,
                          enum fhttpd_config config, void *value)
{
    if (!server || config < 0 || config >= __FHTTPD_CONFIG_MAX)
        return;

    server->config[config] = value;
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

    if (getpid () != server->master_pid)
        {
            return;
        }

    if (server->workers)
        {
            for (size_t i = 0; i < server->num_workers; i++)
                {
                    if (server->workers[i] > 0)
                        {
                            fhttpd_log_warning (
                                "Terminating worker process: %d",
                                server->workers[i]);
                            kill (server->workers[i], SIGTERM);
                        }
                }

            free (server->workers);
        }

    int shmid = server->shmid;

    shmdt (server);
    shmctl (shmid, IPC_RMID, NULL);
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

static int
freehttpd_server_handle_request (struct fhttpd_server *server,
                                 int client_sockfd)
{
    protocol_t protocol = fhttpd_stream_detect_protocol (client_sockfd);

    fhttpd_wclog_info ("Detected protocol: %s",
                       fhttpd_protocol_to_string (protocol));

    if (protocol == FHTTPD_PROTOCOL_UNKNOWN)
        {
            fhttpd_wclog_info ("Unknown protocol detected");
            return ERRNO_GENERIC;
        }

    switch (protocol)
        {
        case FHTTPD_PROTOCOL_H2:
            fhttpd_wclog_info ("Handling h2 request");
            break;

        default:
            fhttpd_wclog_error ("Unsupported protocol: %s",
                                fhttpd_protocol_to_string (protocol));
            close (client_sockfd);
            return ERRNO_GENERIC;
        }

    return ERRNO_SUCCESS;
}

static int
freehttpd_server_enter_loop (struct fhttpd_server *server, int sockfd)
{
    while (true)
        {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof (client_addr);

            int client_sockfd
                = accept (sockfd, (struct sockaddr *) &client_addr, &addr_len);

            if (client_sockfd < 0)
                {
                    if (client_sockfd == -1
                        && (errno == EINTR || errno == EAGAIN))
                        continue;

                    fhttpd_wclog_perror ("accept");
                    continue;
                }

            char client_ip[INET_ADDRSTRLEN];

            if (inet_ntop (AF_INET, &client_addr.sin_addr, client_ip,
                           sizeof (client_ip))
                == NULL)
                {
                    fhttpd_wclog_perror ("inet_ntop");
                    close (client_sockfd);
                    continue;
                }

            fhttpd_wclog_info ("Accepted connection from %s:%d", client_ip,
                               ntohs (client_addr.sin_port));

            int rc = freehttpd_server_handle_request (server, client_sockfd);

            if (rc < 0)
                {
                    fhttpd_wclog_error ("Error handling request: %s",
                                        rc == ERRNO_GENERIC ? "Generic error"
                                                            : strerror (-rc));
                }

            close (client_sockfd);
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

            if ((ret = fhttpd_socket_listen (sockfd, FHTTPD_DEFAULT_BACKLOG))
                < 0)
                {
                    close (sockfd);
                    return -ret;
                }

            server->sockets = realloc (
                server->sockets, sizeof (int) * (server->num_sockets + 1));

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
fhttpd_print_socket_info (int sockfd, pid_t worker_pid)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof (addr);

    if (getsockname (sockfd, (struct sockaddr *) &addr, &addr_len) < 0)
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

    fhttpd_wlog_info (worker_pid, "Listening on socket %d: bound to %s:%d",
                      sockfd, ip, ntohs (addr.sin_port));
}

static int
fhttpd_server_fork_socket_worker (struct fhttpd_server *server, int sockfd)
{
    pid_t pid = fork ();

    if (pid < 0)
        {
            return -errno;
        }
    else if (pid != 0)
        {
            server->workers = realloc (
                server->workers, sizeof (pid_t) * (server->num_workers + 1));

            if (!server->workers)
                return ERRNO_GENERIC;

            server->workers[server->num_workers++] = pid;
        }
    else
        {
            int ret;

            if ((ret = freehttpd_server_enter_loop (server, sockfd)) < 0)
                {
                    fhttpd_wclog_error (
                        "Failed to enter server loop for socket %d: %s", sockfd,
                        strerror (-ret));
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

    for (size_t i = 0; i < server->num_sockets; i++)
        {
            if ((ret = fhttpd_server_fork_socket_worker (server,
                                                         server->sockets[i]))
                < 0)
                {
                    fhttpd_log_error (
                        "Failed to fork child process for socket %d: %s",
                        server->sockets[i], strerror (-ret));
                    return ret;
                }

            fhttpd_print_socket_info (server->sockets[i],
                                      server->workers[server->num_workers - 1]);
        }

    while (true)
        {
            pause ();
        }

    return ERRNO_SUCCESS;
}