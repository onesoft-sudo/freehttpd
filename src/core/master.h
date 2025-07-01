#ifndef FH_CORE_MASTER_H
#define FH_CORE_MASTER_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

struct fh_master
{
    pid_t *worker_pids;
    size_t worker_count;
};

struct fh_master *fh_master_create (void);
void fh_master_destroy (struct fh_master *master);
bool fh_master_spawn_workers (struct fh_master *master);
void fh_master_wait (struct fh_master *master);

#endif /* FH_CORE_MASTER_H */