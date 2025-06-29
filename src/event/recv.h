#ifndef FH_RECV_H
#define FH_RECV_H

#include <stdbool.h>

#include "xpoll.h"
#include "core/server.h"
#include "core/conn.h"

bool fh_event_recv (struct fhttpd_server *server, struct fh_conn *conn);

#endif /* FH_RECV_H */