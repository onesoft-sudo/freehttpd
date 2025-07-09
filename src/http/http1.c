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

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "http1"

#include "compat.h"
#include "core/stream.h"
#include "http1.h"
#include "log/log.h"
#include "macros.h"
#include "mm/pool.h"
#include "utils/strutils.h"
#include "utils/utils.h"

#define H1_RET(ret) ((1 << 8) | (ret))
#define H1_NEXT 0x0
#define H1_RECV 0x1

static const char *HTTP1_METHOD_LIST[] = {
	[FH_METHOD_GET] = "GET",		 [FH_METHOD_POST] = "POST",		  [FH_METHOD_PUT] = "PUT",
	[FH_METHOD_PATCH] = "PATCH",	 [FH_METHOD_DELETE] = "DELETE",	  [FH_METHOD_HEAD] = "HEAD",
	[FH_METHOD_OPTIONS] = "OPTIONS", [FH_METHOD_CONNECT] = "CONNECT", [FH_METHOD_TRACE] = "TRACE",
};

static const size_t HTTP1_METHOD_LIST_SIZE = sizeof (HTTP1_METHOD_LIST) / sizeof HTTP1_METHOD_LIST[0];

struct fh_http1_ctx *
fh_http1_ctx_create (struct fh_stream *stream)
{
	struct fh_http1_ctx *ctx = fh_pool_zalloc (stream->pool, sizeof (*ctx));

	if (!ctx)
		return NULL;

	ctx->state = H1_STATE_METHOD;
	ctx->stream = stream;
	ctx->start = ctx->end = stream->head;

	return ctx;
}

bool 
fh_http1_parse (struct fh_http1_ctx *ctx, struct fh_conn *conn)
{
	return false;
}