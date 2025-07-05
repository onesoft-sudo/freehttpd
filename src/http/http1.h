#ifndef FH_HTTP1_H
#define FH_HTTP1_H

#include <stdbool.h>
#include "core/conn.h"
#include "core/stream.h"
#include "mm/pool.h"

#define HTTP1_METHOD_MAX_LEN 16
#define HTTP1_URI_MAX_LEN   4096

enum http1_state
{
	H1_STATE_METHOD,
	H1_STATE_URI,
	H1_STATE_RECV,
	H1_STATE_ERROR,
	H1_STATE_DONE
};

struct fh_http1_ctx
{
	pool_t *pool;
	struct fh_stream *stream;

	struct fh_link *start, *end;
	size_t start_pos;
	size_t phase_start_pos;

    size_t total_consumed_size;
    size_t current_consumed_size;

	size_t recv_limit;

	enum http1_state state, prev_state;

	const char *method;
	size_t method_len;

	const char *uri;
	size_t uri_len;
};

struct fh_http1_ctx *fh_http1_ctx_create (pool_t *pool, struct fh_stream *stream);
bool fh_http1_parse (struct fh_http1_ctx *ctx, struct fh_conn *conn);
bool fh_http1_is_done (const struct fh_http1_ctx *ctx);

#endif /* FH_HTTP1_H */

