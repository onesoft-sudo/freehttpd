#ifndef FH_ACCEPT_H
#define FH_ACCEPT_H

#include <stdbool.h>

#include "xpoll.h"
#include "core/server.h"

bool fh_event_accept (struct fhttpd_server *server, const xevent_t *event);

#endif /* FH_ACCEPT_H */
