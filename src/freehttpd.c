#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "server.h"

static struct fhttpd_server *server = NULL;

void
exit_handler (void)
{
    fprintf (stderr, "Exiting server...\n");

    if (server)
    {
        fhttpd_server_destroy (server);
        server = NULL;
    }
}

void
signal_handler (int signum)
{
    fprintf (stderr, "\n%s, shutting down %s...\n", strsignal (signum),
             fhttpd_server_get_master_pid (server) == getpid () ? "server"
                                                                : "worker");

    if (fhttpd_server_get_master_pid (server) != getpid ())
    {
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset (&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction (signum, &sa, NULL);
    }

    if (server)
    {
        fhttpd_server_destroy (server);
        server = NULL;
    }

    _exit (0);
}

int
main (void)
{
    atexit (&exit_handler);

    fhttpd_log_set_output (stderr, stdout);

    server = fhttpd_server_create ();

    if (!server)
    {
        fprintf (stderr, "Failed to create server\n");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = &signal_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction (SIGINT, &sa, NULL) < 0 || sigaction (SIGTERM, &sa, NULL) < 0)
    {
        fprintf (stderr, "Failed to set up signal handlers\n");
        return 1;
    }

    uint16_t ports[] = { 8080, 0 };

    fhttpd_server_set_config (server, FHTTPD_CONFIG_PORTS, ports);
    fhttpd_server_set_config (server, FHTTPD_CONFIG_CLIENT_RECV_TIMEOUT,
                              &(uint32_t) { 10000 });
    fhttpd_server_set_config (server, FHTTPD_CONFIG_CLIENT_HEADER_TIMEOUT,
                              &(uint32_t) { 20000 });
    fhttpd_server_set_config (server, FHTTPD_CONFIG_CLIENT_BODY_TIMEOUT,
                              &(uint32_t) { 30000 });

    int ret = fhttpd_server_run (server);

    if (ret < 0)
    {
        fprintf (stderr, "Failed to run server: %s\n",
                 ret == ERRNO_GENERIC ? "Generic error" : strerror (-ret));
        return 1;
    }

    return 0;
}