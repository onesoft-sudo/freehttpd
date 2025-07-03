#ifndef FH_EVENT_ACCEPT_H
#define FH_EVENT_ACCEPT_H

#include <netinet/in.h>
#include <stdbool.h>
#include "types.h"
#include "core/server.h"
#include "xpoll.h"

bool event_accept (struct fh_server *server, const xevent_t *event, const struct sockaddr_in *server_addr);

#endif /* FH_EVENT_ACCEPT_H */