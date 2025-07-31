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

#ifndef FH_CORE_CONN_H
#define FH_CORE_CONN_H

#include <stdint.h>
#include <sys/types.h>
#include <stddef.h>
#include <netinet/in.h>
#include "types.h"
#include "mm/pool.h"
#include "stream.h"
#include "http/protocol.h"

struct fh_requests
{
    struct fh_request *head;
    struct fh_request *tail;
    size_t count;
};

struct fh_conn_extra
{
    const char *host;
    size_t host_len;
    size_t full_host_len;
    uint16_t port;
};

struct fh_conn
{
    object_id_t id;
    fd_t client_sockfd;
	protocol_t protocol;
    struct sockaddr_in *client_addr;
    const struct sockaddr_in *server_addr;
    pool_t *pool;
    struct fh_stream *stream;
    struct fh_requests *requests;
    struct fh_conn_extra *extra;
    const struct fh_config_host *config;

    union {
		struct {
			char *buf;
			size_t off;
		} proto_det_buf;

        struct {
            struct fh_http1_req_ctx *req_ctx;
            struct fh_http1_res_ctx *res_ctx;
        } h1;
    } io_ctx;
};

struct fh_conn *fh_conn_create (fd_t client_sockfd, const struct sockaddr_in *client_addr, const struct sockaddr_in *server_addr);
void fh_conn_destroy (struct fh_conn *conn);
void fh_conn_push_request (struct fh_requests *requests, struct fh_request *request);
struct fh_request *fh_conn_pop_request (struct fh_requests *requests);
bool fh_conn_send_err_response (struct fh_conn *conn, enum fh_status code);
int fh_conn_detect_protocol (struct fh_conn *conn);

#endif /* FH_CORE_CONN_H */
