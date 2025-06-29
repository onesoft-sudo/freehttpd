#define _GNU_SOURCE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "core/conn.h"
#include "core/server.h"
#include "recv.h"
#include "utils/utils.h"

static bool
fh_event_recv_http1 (struct fhttpd_server *server, struct fh_conn *conn)
{
    struct http1_parser_ctx *ctx = &conn->ioh.http1.http1_req_ctx;

	if (!http1_parse (conn, ctx) || ctx->state == HTTP1_STATE_ERROR)
	{
		fhttpd_wclog_error ("connection #%lu: could not parse request", conn->id);
		fhttpd_server_conn_close (server, conn);
		return false;
	}

	if (ctx->state != HTTP1_STATE_DONE || would_block ())
		return true;

	fhttpd_wclog_info ("Switching to generation of response phase");

    xpoll_mod (server->xpoll, conn->client_sockfd, XPOLLOUT);
	return true;
}

bool
fh_event_recv (struct fhttpd_server *server, struct fh_conn *conn)
{
	if (conn->protocol == FHTTPD_PROTOCOL_UNKNOWN)
	{
		if (!fh_conn_detect_protocol (conn))
		{
			fhttpd_wclog_error ("connection #%lu: failed to detect protocol", conn->id);
			fhttpd_server_conn_close (server, conn);
			return false;
		}

		if (would_block ())
			return true;

		static_assert (sizeof (conn->proto_detect_buffer) < sizeof (conn->ioh.http1.http1_req_ctx.buffer),
					   "The HTTP1 buffer is too small");

		switch (conn->protocol)
		{
			case FHTTPD_PROTOCOL_HTTP_1_0:
			case FHTTPD_PROTOCOL_HTTP_1_1:
				conn->requests = calloc (1, sizeof (struct fhttpd_request));

				if (!conn->requests)
				{
					fhttpd_wclog_error ("connection #%lu: failed to allocate memory", conn->id);
					fhttpd_server_conn_close (server, conn);
					return false;
				}

				conn->request_cap = 1;
				conn->request_count = 0;

				if (!fhttpd_request_init (conn, &conn->requests[0]))
				{
					fhttpd_wclog_error ("connection #%lu: failed to allocate memory for requests", conn->id);
					fhttpd_server_conn_close (server, conn);
					return false;
				}

				http1_parser_ctx_init (conn->requests[0].pool, &conn->ioh.http1.http1_req_ctx);
				memcpy (conn->ioh.http1.http1_req_ctx.buffer, conn->proto_detect_buffer,
						conn->proto_detect_buffer_size);
				conn->ioh.http1.http1_req_ctx.buffer_len = conn->proto_detect_buffer_size;
				break;

			default:
				fhttpd_wclog_error ("connection #%lu: unsupported protocol %d", conn->id, conn->protocol);
				fhttpd_server_conn_close (server, conn);
				return false;
		}

		fhttpd_wclog_debug ("connection #%lu: detected protocol: %s", conn->id,
							fhttpd_protocol_to_string (conn->protocol));
	}

	switch (conn->protocol)
	{
		case FHTTPD_PROTOCOL_HTTP_1_0:
		case FHTTPD_PROTOCOL_HTTP_1_1:
			return fh_event_recv_http1 (server, conn);

		default:
			fhttpd_wclog_error ("connection #%lu: unsupported protocol %d", conn->id, conn->protocol);
			fhttpd_server_conn_close (server, conn);
			return false;
	}
}
