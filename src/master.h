#ifndef FHTTPD_MASTER_H
#define FHTTPD_MASTER_H

#include <stdbool.h>
#include <sys/types.h>

#include "conf.h"
#include "types.h"

struct fhttpd_notify_stat
{
	uint64_t current_connection_count;
	uint64_t total_connection_count;
};

enum fhttpd_notification
{
	FHTTPD_NOTIFY_STAT
};

struct fhttpd_master
{
	pid_t pid;
	pid_t *workers;
	fd_t (*worker_pipes)[2];
	struct fhttpd_notify_stat *worker_stats;
	size_t worker_count;
	struct fhttpd_config *config;
};

struct fhttpd_master *fhttpd_master_create (void);
bool fhttpd_master_start (struct fhttpd_master *master);
void fhttpd_master_destroy (struct fhttpd_master *master);
bool fhttpd_master_prepare (struct fhttpd_master *master);
bool fhttpd_master_load_config (struct fhttpd_master *master);
bool fhttpd_master_reload_config (struct fhttpd_master *master);

#endif /* FHTTPD_MASTER_H */
