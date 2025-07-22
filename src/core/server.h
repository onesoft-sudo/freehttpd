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

#ifndef FH_CORE_SERVER_H
#define FH_CORE_SERVER_H

#include <stdbool.h>
#include <stddef.h>

#include "types.h"
#include "conf.h"
#include "confproc.h"
#include "event/xpoll.h"
#include "hash/strtable.h"
#include "hash/itable.h"
#include "conn.h"

#define FH_SERVER_MAX_SOCKETS 128

struct fh_module_manager;

struct fh_server
{
    struct fh_config *config;
    fd_t xpoll_fd;
    bool should_exit : 1;

    /* (fd_t) => (struct sockaddr_in *) */
    struct itable *sockfd_table;

    /* (fd_t) => (struct fh_conn *) */
    struct itable *connections;

    /* (const char *) => (struct fh_config_host *) */
    struct strtable *host_configs;

    struct fh_router *router;
	struct fh_module_manager *module_manager;
};

struct fh_server *fh_server_create (struct fh_config *config, struct fh_module_manager *module_manager);
void fh_server_destroy (struct fh_server *server);
void fh_server_loop (struct fh_server *server);
bool fh_server_listen (struct fh_server *server);
void fh_server_close_conn (struct fh_server *server, struct fh_conn *conn);

#endif /* FH_CORE_SERVER_H */
