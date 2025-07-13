/*
 * This file is part of OSN freehttpd.
 * 
 * Copyright (C) 2025  OSN Developers.
 *
 * OSN freehttpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * OSN freehttpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef FH_CORE_MASTER_H
#define FH_CORE_MASTER_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#include "conf.h"

struct fh_master
{
    pid_t *worker_pids;
    size_t worker_count;
    struct fh_config *config;
};

struct fh_master *fh_master_create (void);
bool fh_master_setup_signal (struct fh_master *master);
void fh_master_destroy (struct fh_master *master);
bool fh_master_spawn_workers (struct fh_master *master);
void fh_master_wait (struct fh_master *master);
bool fh_master_read_config (struct fh_master *master);

#endif /* FH_CORE_MASTER_H */
