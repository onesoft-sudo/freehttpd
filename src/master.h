#ifndef FHTTPD_MASTER_H
#define FHTTPD_MASTER_H

#include <stdbool.h>
#include <sys/types.h>

#include "conf.h"

struct fhttpd_master
{
	pid_t pid;
	pid_t *workers;
	size_t worker_count;
	struct fhttpd_config *config;
};

struct fhttpd_master *fhttpd_master_create (void);
bool fhttpd_master_start (struct fhttpd_master *master);
void fhttpd_master_destroy (struct fhttpd_master *master);
bool fhttpd_master_prepare (struct fhttpd_master *master);

#endif /* FHTTPD_MASTER_H */
