#ifndef FH_HTTP1_H
#define FH_HTTP1_H

#include <stdbool.h>
#include "core/conn.h"
#include "core/stream.h"
#include "mm/pool.h"

#define HTTP1_METHOD_MAX_LEN 16
#define HTTP1_VERSION_MAX_LEN 8
#define HTTP1_URI_MAX_LEN   4096
#define HTTP1_HEADER_NAME_MAX_LEN 128
#define HTTP1_HEADER_VALUE_MAX_LEN 256
#define HTTP1_HEADER_COUNT_MAX 128

enum http1_state
{
	H1_STATE_METHOD,
	H1_STATE_URI,
	H1_STATE_VERSION,
	H1_STATE_HEADER_NAME,
	H1_STATE_HEADER_VALUE,
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

	const char *current_header_name;
	size_t current_header_name_len;

	const char *method;
	size_t method_len;

	const char *uri;
	size_t uri_len;

	const char *version;
	size_t version_len;
};

struct fh_http1_ctx *fh_http1_ctx_create (pool_t *pool, struct fh_stream *stream);
bool fh_http1_parse (struct fh_http1_ctx *ctx, struct fh_conn *conn);
bool fh_http1_is_done (const struct fh_http1_ctx *ctx);

#endif /* FH_HTTP1_H */

