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

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "main"

#include "log/log.h"
#include "core/master.h"

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef FHTTPD_ENABLE_SYSTEMD
	#include <systemd/sd-daemon.h>
#endif /* FHTTPD_ENABLE_SYSTEMD */

int
main (int argc, char **argv)
{
	if (argc > 1)
	{
		fprintf (stderr, "%s: invalid argument -- '%s'\n", argv[0], argv[1]);
		return 1;
	}
	
	struct fh_master *master = fh_master_create ();

	if (!fh_master_read_config (master))
		return 1;

	if (!fh_master_setup_signal (master))
	{
		fh_pr_err ("failed to setup signal handlers: %s", strerror (errno));
		return 1;
	}
	
	if (!fh_master_spawn_workers (master))
	{
		fh_pr_err ("failed to spawn worker processes: %s", strerror (errno));
		return 1;
	}

	fh_master_wait (master);

	fh_pr_warn ("All worker processes were terminated, exiting");
	fh_master_destroy (master);
	return 0;
}
