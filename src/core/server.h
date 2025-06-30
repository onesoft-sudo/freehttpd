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

#ifndef FHTTPD_SERVER_H
#define FHTTPD_SERVER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

#include "compat.h"
#include "conf.h"
#include "master.h"
#include "types.h"
#include "utils/itable.h"
#include "http/protocol.h"
#include "utils/strtable.h"
#include "event/xpoll.h"

#define MAX_REQUEST_THREADS 4

struct fhttpd_addrinfo
{
	struct sockaddr_in addr;
	socklen_t addr_len;
	char host_addr[INET_ADDRSTRLEN];
	uint16_t port;
	fd_t sockfd;
};

struct fhttpd_server
{
	pid_t master_pid;
	pid_t pid;

	xpoll_t xpoll;
	fd_t timer_fd;

	fd_t *listen_fds;
	size_t listen_fd_count;

	struct fhttpd_config *config;

	/* (const char *) => (struct fhttpd_config_host *) */
	struct strtable *host_config_table;

	/* (fd_t) => (struct fhttpd_addrinfo *) */
	struct itable *sockaddr_in_table;

	/* (fd_t) => (struct fhttpd_connection *) */
	struct itable *connections;
	uint64_t last_connection_id;

	struct fh_conn *conn_pool_free_list;
	struct fh_conn *conn_pool;
	size_t conn_pool_size;
	size_t conn_count;

	fd_t pipe_fd[2];

	bool flag_terminate : 1;
	bool flag_clean_quit : 1;
};

_noreturn void fhttpd_server_loop (struct fhttpd_server *server);

struct fhttpd_server *fhttpd_server_create (const struct fhttpd_master *master, struct fhttpd_config *config, fd_t pipe_fd[static 2]);
bool fhttpd_server_prepare (struct fhttpd_server *server);
void fhttpd_server_destroy (struct fhttpd_server *server);
void fhttpd_server_config_host_map (struct fhttpd_server *server);
bool fhttpd_server_conn_close (struct fhttpd_server *server, struct fh_conn *conn);


struct fh_conn *fhttpd_server_acquire_conn (struct fhttpd_server *server);
void fhttpd_server_release_conn (struct fhttpd_server *server, struct fh_conn *conn);

#endif /* FHTTPD_SERVER_H */
