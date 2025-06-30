#define _GNU_SOURCE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define FHTTPD_LOG_MODULE_NAME "recv"

#include "core/conn.h"
#include "core/server.h"
#include "core/stream.h"
#include "log/log.h"
#include "recv.h"
#include "utils/utils.h"

static bool
fh_event_recv_http1 (struct fhttpd_server *server, struct fh_conn *conn)
{
	fhttpd_server_conn_close (server, conn);
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

		static_assert (sizeof (conn->proto_detect_buffer) < sizeof (conn->io_h.http1.http1_req_ctx->buffer),
					   "The HTTP1 buffer is too small");

		switch (conn->protocol)
		{
			case FHTTPD_PROTOCOL_HTTP_1_0:
			case FHTTPD_PROTOCOL_HTTP_1_1:
				{
					conn->requests = calloc (1, sizeof (struct fhttpd_request));

					if (!conn->requests)
					{
						fhttpd_wclog_error ("connection #%lu: failed to allocate memory", conn->id);
						fhttpd_server_conn_close (server, conn);
						return false;
					}

					conn->request_cap = 1;
					conn->request_count = 0;

					struct fhttpd_request *request = &conn->requests[0];

					if (!fhttpd_request_init (conn, request))
					{
						fhttpd_wclog_error ("connection #%lu: failed to allocate memory for requests", conn->id);
						fhttpd_server_conn_close (server, conn);
						return false;
					}

					struct fh_chain *first_chain;

					if (!(first_chain = fh_stream_append_chain_memcpy (request->stream, conn->proto_detect_buffer,
																	   conn->proto_detect_buffer_size)))
					{
						fhttpd_wclog_error ("connection #%lu: failed to create initial stream chain", conn->id);
						fhttpd_server_conn_close (server, conn);
						return false;
					}

					first_chain->is_start = true;
				}

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
