/*
 * This file is part of OSN freehttpd.
 * 
 * Copyright (C) 2025  OSN Developers.
 *
 * OSN freehttpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * OSN freehttpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.
 */

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
