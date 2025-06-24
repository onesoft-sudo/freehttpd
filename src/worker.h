#ifndef FHTTPD_WORKER_H
#define FHTTPD_WORKER_H

#include "types.h"
#include "compat.h"

struct fhttpd_master;

__noreturn void fhttpd_worker_start (struct fhttpd_master *master, fd_t pipe_fd[static 2]);

#endif /* FHTTPD_WORKER_H */