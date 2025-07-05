#define FH_LOG_MODULE_NAME "event/recv"

#include <stdlib.h>
#include <unistd.h>

#include "compat.h"
#include "core/conn.h"
#include "core/server.h"
#include "core/stream.h"
#include "http/http1.h"
#include "log/log.h"
#include "recv.h"

/*
	struct fh_conn *conn = itable_get (server->connections, event->data.fd);

	if (!conn)
	{
		fh_pr_err ("Socket %d does not have an associated connection object", event->data.fd);
		xpoll_del (server->xpoll_fd, event->data.fd, XPOLLIN);
		close (event->data.fd);
		return false;
	}

	for (;;)
	{
		size_t cap = 4096;
		uint8_t *buf;

		if (conn->stream->tail && conn->stream->tail->buf->len < conn->stream->tail->buf->cap)
		{
			buf = conn->stream->tail->buf->data + conn->stream->tail->buf->len;
			cap = conn->stream->tail->buf->cap - conn->stream->tail->buf->len;
		}

		buf = fh_pool_alloc (conn->pool, cap);

		if (!buf)
		{
			fh_pr_debug ("connection %lu: buffer allocation failed: %s", conn->id, strerror (errno));
			fh_server_close_conn (server, conn);
			return false;
		}

		ssize_t bytes_received = recv (conn->client_sockfd, buf, cap, 0);

		if (bytes_received < 0)
		{
			if (would_block ())
				break;

			fh_pr_debug ("connection %lu: I/O error: %s", conn->id, strerror (errno));
			fh_server_close_conn (server, conn);
			return false;
		}

		if (bytes_received == 0)
			break;



		fh_pr_debug ("connection %lu: received %zu bytes", conn->id, (size_t) bytes_received);
	}

	fh_server_close_conn (server, conn);*/

bool
event_recv (struct fh_server *server, const xevent_t *event)
{
	struct fh_conn *conn = itable_get (server->connections, event->data.fd);

	if (!conn)
	{
		fh_pr_err ("Socket %d does not have an associated connection object", event->data.fd);
		xpoll_del (server->xpoll_fd, event->data.fd, XPOLLIN);
		close (event->data.fd);
		return false;
	}

	fh_pr_info ("connection %lu: recv called", conn->id);

	if (!conn->req_ctx)
		conn->stream = fh_stream_new (conn->pool);
	
	struct fh_http1_ctx *ctx = conn->req_ctx ? conn->req_ctx : fh_http1_ctx_create (conn->pool, conn->stream);

	if (!conn->req_ctx)
		conn->req_ctx = ctx;

	int iter = 0;

	for (;;)
	{
		if (iter > 100)
			break;

		iter++;

		if (!fh_http1_parse (ctx, conn))
			break;

		if (ctx->state == H1_STATE_DONE || ctx->state == H1_STATE_ERROR)
			break;
	}

	if (ctx->state == H1_STATE_DONE)
	{
		fh_pr_info ("Method: |%.*s|", (int) ctx->method_len, ctx->method);
		fh_pr_info ("URI: |%.*s|", (int) ctx->uri_len, ctx->uri);
		
		if (!xpoll_mod (server->xpoll_fd, conn->client_sockfd, XPOLLOUT))
		{
			fh_pr_err ("Unable to switch to write mode");
			fh_server_close_conn (server, conn);	
		}
	}
	else if (ctx->state == H1_STATE_ERROR)
	{
		fh_pr_err ("HTTP/1.x parsing failed");
		fh_server_close_conn (server, conn);
	}
	else 
	{
		fh_pr_info ("HTTP/1.x parsing did not finish yet");
	}

	return true;
}
