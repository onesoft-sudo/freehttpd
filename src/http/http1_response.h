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

#ifndef FH_HTTP1_RESPONSE_H
#define FH_HTTP1_RESPONSE_H

#include <stdbool.h>

#include "core/conn.h"
#include "core/server.h"
#include "core/stream.h"
#include "http1.h"
#include "mm/pool.h"
#include "protocol.h"

enum fh_http1_res_state
{
	FH_RES_STATE_HEADERS,
	FH_RES_STATE_BODY,
	FH_RES_STATE_ERROR,
	FH_RES_STATE_DONE,
	FH_RES_STATE_WRITE
};

struct fh_http1_res_ctx
{
	pool_t *pool;
	uint8_t state : 4;
	uint8_t next_state : 4;
	struct iovec *iov;
	size_t iov_size, iov_data_size;
	struct fh_link *link;
	struct fh_response *response;
};

struct fh_http1_res_ctx *fh_http1_res_ctx_create (pool_t *pool);
struct fh_http1_res_ctx *
fh_http1_res_ctx_create_with_response (pool_t *pool,
									   struct fh_response *response);
bool fh_http1_send_response (struct fh_http1_res_ctx *ctx,
							 struct fh_conn *conn);
void fh_http1_res_ctx_clean (struct fh_http1_res_ctx *ctx);

#endif /* FH_HTTP1_RESPONSE_H */
