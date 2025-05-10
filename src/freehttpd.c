#include <stdio.h>
#include <stdint.h>

#include "server.h"
#include "lstring.h"

int main(void) 
{
    struct fhttpd_server *server = fhttpd_server_create();

    if (!server) 
    {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }

    uint16_t port = 8080;

    if (!fhttpd_server_set_config(server, FHTTPD_CONFIG_PORT, &port)) 
    {
        fprintf(stderr, "Failed to set server config\n");
        fhttpd_server_destroy(server);
        return 1;
    }

    if (fhttpd_server_initialize(server) != 0) 
    {
        fprintf(stderr, "Failed to initialize server\n");
        fhttpd_server_destroy(server);
        return 1;
    }

    if (fhttpd_server_start(server) != 0) 
    {
        fprintf(stderr, "Failed to start server\n");
        fhttpd_server_destroy(server);
        return 1;
    }

    return 0;
}