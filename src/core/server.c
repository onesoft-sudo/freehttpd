#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "server.h"
#include "conf.h"

struct fh_server *
fh_server_create (struct fhttpd_config *config)
{
    struct fh_server *server = calloc (1, sizeof (struct fh_server));

    if (!server)
        return NULL;
    
    server->config = config;
    return server;
}

void 
fh_server_destroy (struct fh_server *server)
{
    fhttpd_conf_free_config (server->config);
    free (server);
}

bool 
fh_server_listen (struct fh_server *server)
{
    return true;
}

void 
fh_server_loop (struct fh_server *server)
{
    while (1) {
        if (server->should_exit)
            return;

        pause ();
    }
}