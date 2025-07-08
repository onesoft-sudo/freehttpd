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

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define FH_LOG_MODULE_NAME "conn"

#include "conn.h"
#include "log/log.h"

static object_id_t next_conn_id = 0;

struct fh_conn *
fh_conn_create (fd_t client_sockfd, const struct sockaddr_in *client_addr, const struct sockaddr_in *server_addr)
{
    pool_t *pool = fh_pool_create (0);

    if (!pool)
        return NULL;

    struct fh_conn *conn = fh_pool_alloc (pool, sizeof (*conn) + sizeof (*client_addr) + sizeof (*conn->stream));

    if (!conn)
        return NULL;

    conn->id = next_conn_id++;
    conn->client_addr = (struct sockaddr_in *) (conn + 1);
    conn->stream = (struct fh_stream *) (conn->client_addr + 1);
    conn->client_sockfd = client_sockfd;
    conn->pool = pool;
    conn->server_addr = server_addr;
    conn->req_ctx = NULL;

    return conn;
}

void
fh_conn_destroy (struct fh_conn *conn)
{
    pool_t *pool = conn->pool;
    fh_pr_debug ("Connection #%lu will now be deallocated", conn->id);
    close (conn->client_sockfd);
    fh_pool_destroy (pool);
}
