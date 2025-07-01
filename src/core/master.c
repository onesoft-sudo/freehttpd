#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define FH_LOG_MODULE "master"

#include "master.h"
#include "log/log.h"

#define FH_MASTER_SPAWN_WORKERS 4

struct fh_master *
fh_master_create (void)
{
	return calloc (1, sizeof (struct fh_master));
}

void
fh_master_destroy (struct fh_master *master)
{
	free (master->worker_pids);
	free (master);
}

bool
fh_master_spawn_workers (struct fh_master *master)
{
    master->worker_pids = calloc (FH_MASTER_SPAWN_WORKERS, sizeof (pid_t));

    if (!master->worker_pids)
        return false;

    master->worker_count = FH_MASTER_SPAWN_WORKERS;

    for (size_t i = 0; i < FH_MASTER_SPAWN_WORKERS; i++)
    {
        pid_t pid = fork ();

        if (pid < 0)
            return false;
            
        if (pid == 0)
        {
            free (master->worker_pids);
	        free (master);
            fh_pr_info ("Hello world!");
            fh_pr_info ("Hello world!");
            fh_pr_info ("Hello world!");
            fh_pr_info ("Hello world!");
            _exit (0);
        }
        else
        {
            master->worker_pids[i] = pid;
        }
    }

    return true;
}

void
fh_master_wait (struct fh_master *master)
{
    for (size_t i = 0; i < master->worker_count; i++)
        waitpid (master->worker_pids[i], NULL, 0);
}