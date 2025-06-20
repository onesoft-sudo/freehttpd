#ifndef FHTTPD_MASTER_H
#define FHTTPD_MASTER_H

#include <sys/types.h>
#include <stdbool.h>

#include "conf.h"

struct fhttpd_master
{
    pid_t pid;
    pid_t *workers;
    size_t worker_count;
    void *config[FHTTPD_CONFIG_MAX];
};

struct fhttpd_master *fhttpd_master_create (void);
bool fhttpd_master_start (struct fhttpd_master *master);
void fhttpd_master_destroy (struct fhttpd_master *master);

#endif /* FHTTPD_MASTER_H */