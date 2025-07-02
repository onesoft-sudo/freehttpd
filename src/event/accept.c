#define _GNU_SOURCE

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define FH_LOG_MODULE_NAME "event/accept"

#include "compat.h"
#include "accept.h"
#include "log/log.h"

bool 
event_accept (struct fh_server *server, const xevent_t *ev_info)
{
    const fd_t sockfd = ev_info->data.fd;
    size_t errors = 0;

    for (;;)
    {
        struct sockaddr_in client_addr = {0};
        socklen_t client_addr_len = sizeof client_addr;

#if defined(FH_PLATFORM_LINUX)
        fd_t client_sockfd = accept4 (sockfd, &client_addr, &client_addr_len, SOCK_NONBLOCK);
#elif defined(FH_PLATFORM_BSD)
        fd_t client_sockfd = accept (sockfd, &client_addr, &client_addr_len);
#endif

        if (client_sockfd < 0)
        {
            if (errno == EINTR)
                continue;

            if (would_block ())
                return true;

            fh_pr_err ("accept syscall failed: %s", strerror (errno));

            if (errors >= 5)
                return false;

            errors++;
            continue;
        }

        errors = 0;

        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop (AF_INET, &client_addr.sin_addr.s_addr, ip, sizeof ip);
        uint16_t port = ntohs (client_addr.sin_port);

        fh_pr_info ("accepted new connection from %s:%u", ip, port);

        usleep (1000000);

        fh_pr_info ("connection closed");
        close (client_sockfd);
    }

    return true;
}