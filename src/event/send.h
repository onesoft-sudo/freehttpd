#ifndef FH_SEND_H
#define FH_SEND_H

#include <stdbool.h>

#include "core/server.h"
#include "core/conn.h"

bool fh_event_send (struct fhttpd_server *server, struct fh_conn *conn);

#endif /* FH_SEND_H */