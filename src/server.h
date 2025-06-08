#ifndef FHTTPD_SERVER_H
#define FHTTPD_SERVER_H

#include <sys/types.h>

#define ERRNO_GENERIC   -256
#define ERRNO_SUCCESS      0

enum fhttpd_config
{
    FHTTPD_CONFIG_PORTS,
    FHTTPD_CONFIG_DOCROOT,
    FHTTPD_CONFIG_MAX_CONNECTIONS,
    FHTTPD_CONFIG_TIMEOUT,
    FHTTPD_CONFIG_LOG_LEVEL,
    __FHTTPD_CONFIG_MAX
};

struct fhttpd_server;

struct fhttpd_server *fhttpd_server_create(void);
void fhttpd_server_destroy(struct fhttpd_server *server);
int fhttpd_server_run(struct fhttpd_server *server);
void fhttpd_server_set_config(struct fhttpd_server *server,
                              enum fhttpd_config config, void *value);
pid_t fhttpd_server_get_master_pid(struct fhttpd_server *server);

#endif /* FHTTPD_SERVER_H */