#ifndef FREEHTTPD_SERVER_H
#define FREEHTTPD_SERVER_H

#include <stdbool.h>
#include "types.h"

struct fhttpd_server;

enum fhttpd_server_config
{
    FHTTPD_CONFIG_PORT,
    FHTTPD_CONFIG_BIND_ADDR,
    __FHTTPD_CONFIG_COUNT
};

struct fhttpd_server *fhttpd_server_create();
void fhttpd_server_destroy(struct fhttpd_server *server);
errno_t fhttpd_server_initialize(struct fhttpd_server *server);
errno_t fhttpd_server_start(struct fhttpd_server *server);
bool fhttpd_server_set_config(struct fhttpd_server *server, enum fhttpd_server_config config, void *value);
void *fhttpd_server_get_config(struct fhttpd_server *server, enum fhttpd_server_config config);

#endif /* FREEHTTPD_SERVER_H */