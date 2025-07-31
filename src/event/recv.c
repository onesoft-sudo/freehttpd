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

#define FH_LOG_MODULE_NAME "event/recv"

#include <stdlib.h>
#include <unistd.h>

#include "compat.h"
#include "core/conn.h"
#include "core/server.h"
#include "core/stream.h"
#include "http/http1_request.h"
#include "http/protocol.h"
#include "log/log.h"
#include "recv.h"

static bool
event_recv_h2 (struct fh_server *server, struct fh_conn *conn,
			   char *proto_det_buf, size_t proto_det_off)
{
	(void) proto_det_buf;
	(void) proto_det_off;

	fh_pr_err ("Not implemented: h2");
	fh_server_close_conn (server, conn);
	return false;
}

static bool
event_recv_http1 (struct fh_server *server, struct fh_conn *conn,
				  char *proto_det_buf, size_t proto_det_off)
{
	pool_t *child_pool = NULL;

	if (!conn->io_ctx.h1.req_ctx)
	{
		child_pool = fh_pool_create (0);

		if (!child_pool)
		{
			fh_pr_err ("Failed to allocate memory");
			fh_server_close_conn (server, conn);
			return false;
		}

		fh_stream_init (conn->stream, child_pool);

		if (fh_stream_add_buf_data (conn->stream, (uint8_t *) proto_det_buf,
									proto_det_off, proto_det_off)
			&& conn->stream->head)
		{
			conn->stream->head->buf->attrs.mem.rd_only = true;
		}
	}

	struct fh_http1_req_ctx *ctx
		= conn->io_ctx.h1.req_ctx
			  ? conn->io_ctx.h1.req_ctx
			  : fh_http1_ctx_create (server, conn, conn->stream);

	if (!conn->io_ctx.h1.req_ctx)
	{
		conn->io_ctx.h1.req_ctx = ctx;
		ctx->cur.link = ctx->arg_cur.link = conn->stream->head;
		ctx->cur.off = ctx->arg_cur.off = 0;
		conn->stream->head->is_start = true;
	}

	if (!fh_http1_parse (ctx, conn))
	{
		if (ctx->state == H1_REQ_STATE_ERROR)
		{
			fh_pr_err ("HTTP/1.x parsing failed");
			fh_conn_send_err_response (
				conn, ctx->suggested_code == 0 ? 500 : ctx->suggested_code);
			fh_server_close_conn (server, conn);

			if (child_pool)
				fh_pool_destroy (child_pool);

			return true;
		}

		fh_pr_err ("HTTP/1.x parser silently failed: this should not happen");
		return true;
	}

	if (ctx->state == H1_REQ_STATE_DONE)
	{
		struct fh_request *request = &ctx->request;
		request->pool = conn->stream->pool;
		fh_conn_push_request (conn->requests, request);

		fh_pr_info ("Method: |%s|", fh_method_to_string (ctx->request.method));
		fh_pr_info ("URI: |%.*s|", (int) ctx->request.uri_len,
					ctx->request.uri);
		fh_pr_info ("Protocol: %s",
					fh_protocol_to_string (ctx->request.protocol));

		if (!xpoll_mod (server->xpoll_fd, conn->client_sockfd, XPOLLOUT))
		{
			fh_pr_err ("Unable to switch to write mode");
			fh_server_close_conn (server, conn);
		}
	}
	else
	{
		fh_pr_info ("HTTP/1.x parsing did not finish yet");
	}

	return true;
}

bool
event_recv (struct fh_server *server, const xevent_t *event)
{
	struct fh_conn *conn = itable_get (server->connections, event->data.fd);

	if (!conn)
	{
		fh_pr_err ("Socket %d does not have an associated connection object",
				   event->data.fd);
		xpoll_del (server->xpoll_fd, event->data.fd, XPOLLIN);
		close (event->data.fd);
		return false;
	}

	fh_pr_info ("connection %lu: recv called", conn->id);

	char *proto_det_buf = NULL;
	size_t proto_det_off = 0;

	if (conn->protocol == FH_PROTOCOL_UNKNOWN)
	{
		/* Detect the connection protocol */

		int rc = fh_conn_detect_protocol (conn);

		switch (rc)
		{
			case 0:
				fh_pr_debug ("EAGAIN while detecting protocol");
				return true;

			case -1:
				fh_pr_debug ("I/O error while detecting protocol");
				fh_server_close_conn (server, conn);
				return true;
		}

		proto_det_buf = conn->io_ctx.proto_det_buf.buf;
		proto_det_off = conn->io_ctx.proto_det_buf.off;

		conn->io_ctx.h1.req_ctx = NULL;
		conn->io_ctx.h1.res_ctx = NULL;

		fh_pr_debug ("Detected protocol: %s",
					 fh_protocol_to_string (conn->protocol));
	}

	switch (conn->protocol)
	{
		case FH_PROTOCOL_HTTP_1_0:
		case FH_PROTOCOL_HTTP_1_1:
			return event_recv_http1 (server, conn, proto_det_buf,
									 proto_det_off);

		case FH_PROTOCOL_H2:
			return event_recv_h2 (server, conn, proto_det_buf, proto_det_off);

		default:
			fh_pr_debug ("Unsupported protocol");
			fh_server_close_conn (server, conn);
			return true;
	}
}
