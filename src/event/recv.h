#ifndef FH_EVENT_RECV_H
#define FH_EVENT_RECV_H

#include <stdbool.h>
#include "types.h"
#include "core/server.h"
#include "xpoll.h"

bool event_recv (struct fh_server *server, const xevent_t *event);

#endif /* FH_EVENT_RECV_H */