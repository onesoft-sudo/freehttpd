#ifndef FHTTPD_WORKER_H
#define FHTTPD_WORKER_H

#include "compat.h"
#include "master.h"

__noreturn void fhttpd_worker_start (struct fhttpd_master *master);

#endif /* FHTTPD_WORKER_H */