#ifndef FH_ROUTER_FILESYSTEM_H
#define FH_ROUTER_FILESYSTEM_H

#include <stdbool.h>

#include "core/stream.h"
#include "core/conn.h"
#include "http/protocol.h"
#include "http/http1_request.h"
#include "http/http1_response.h"

bool fh_router_handle_filesystem (struct fh_router *router, struct fh_conn *conn,
							 const struct fh_request *request,
							 struct fh_response *response);

#endif /* FH_ROUTER_FILESYSTEM_H */