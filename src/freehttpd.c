#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "log.h"
#include "server.h"

static struct fhttpd_master *fhttpd = NULL;

void
exit_handler (void)
{
    if (fhttpd)
        {
            fhttpd_master_destroy (fhttpd);
            fhttpd = NULL;
        }
}

void
signal_handler (int signum)
{
    fprintf (stderr, "%s, shutting down %s...\n", strsignal (signum), fhttpd->pid == getpid () ? "server" : "worker");

    if (fhttpd->pid != getpid ())
    {
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset (&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction (signum, &sa, NULL);
    }

    exit (0);
}

int
main (void)
{
    atexit (&exit_handler);
    fhttpd_log_set_output (stderr, stdout);

    fhttpd = fhttpd_master_create ();

    if (!fhttpd)
        {
            fprintf (stderr, "Failed to create server\n");
            return 1;
        }

    struct sigaction sa;

    sa.sa_handler = &signal_handler;
    sa.sa_flags = 0;
    sigemptyset (&sa.sa_mask);

    if (sigaction (SIGINT, &sa, NULL) < 0 || sigaction (SIGTERM, &sa, NULL) < 0)
        {
            fprintf (stderr, "Failed to set up signal handlers\n");
            return 1;
        }

    uint16_t ports[] = { 8080, 0 };

    fhttpd_set_config (fhttpd, FHTTPD_CONFIG_PORTS, ports);
    fhttpd_set_config (fhttpd, FHTTPD_CONFIG_CLIENT_RECV_TIMEOUT,
                              &(uint32_t) { 10000 });
    fhttpd_set_config (fhttpd, FHTTPD_CONFIG_CLIENT_HEADER_TIMEOUT,
                              &(uint32_t) { 20000 });
    fhttpd_set_config (fhttpd, FHTTPD_CONFIG_CLIENT_BODY_TIMEOUT,
                              &(uint32_t) { 30000 });
    fhttpd_set_config (fhttpd, FHTTPD_CONFIG_WORKER_COUNT,
                              &(size_t) { 4 });

    if (!fhttpd_master_start (fhttpd))
    {
        fprintf (stderr, "Failed to run server: %s\n", strerror (errno));
        return 1;
    }

    return 0;
}
