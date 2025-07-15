#ifndef FH_HTTP1_RESPONSE_H
#define FH_HTTP1_RESPONSE_H

#include <stdbool.h>

#include "core/conn.h"
#include "core/stream.h"
#include "core/server.h"
#include "mm/pool.h"
#include "protocol.h"
#include "http1.h"

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
bool fh_http1_send_response (struct fh_http1_res_ctx *ctx, struct fh_conn *conn);

#endif /* FH_HTTP1_RESPONSE_H */