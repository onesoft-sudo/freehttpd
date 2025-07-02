#ifndef FH_CORE_WORKER_H
#define FH_CORE_WORKER_H

#include "conf.h"
#include "compat.h"

_noreturn void fh_worker_start (struct fhttpd_config *config);

#endif /* FH_CORE_WORKER_H */