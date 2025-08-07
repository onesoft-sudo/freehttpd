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

#define _GNU_SOURCE
#define _DEFAULT_SOURCE

#include <stddef.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>

#define FH_LOG_MODULE_NAME "main"

#include "core/master.h"
#include "log/log.h"
#include "utils/print.h"

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef FHTTPD_ENABLE_SYSTEMD
	#include <systemd/sd-daemon.h>
#endif /* FHTTPD_ENABLE_SYSTEMD */

static const char *invocation_name = NULL;

static struct option const long_options[] = {
	{ "help", no_argument, 0, 'h' },
	{ "version", no_argument, 0, 'V' },
	{ 0, 0, 0, 0 },
};

static const char *short_options = "hV";

static int
start_master (void)
{
	struct fh_master *master = fh_master_create ();

	if (!fh_master_read_config (master))
	{
		fh_master_destroy (master);
		return 1;
	}

	if (!fh_master_load_modules (master))
	{
		fh_pr_err ("failed to load modules: %s", strerror (errno));
		fh_master_destroy (master);
		return 1;
	}

	if (!fh_master_setup_signal (master))
	{
		fh_pr_err ("failed to setup signal handlers: %s", strerror (errno));
		fh_master_destroy (master);
		return 1;
	}

	if (!fh_master_spawn_workers (master))
	{
		fh_pr_err ("failed to spawn worker processes: %s", strerror (errno));
		fh_master_destroy (master);
		return 1;
	}

	fh_master_wait (master);
	fh_pr_warn ("All worker processes were terminated, exiting");
	fh_master_destroy (master);

	return 0;
}

static void
usage (void)
{
	println ("The freehttpd server daemon.");
	println ("");
	println ("Usage:");
	println ("  %s [options]", invocation_name);
	println ("");
	println ("Options:");
	println ("  -h, --help         Print this help and exit");
	println ("  -V, --version      Print version information");
	println ();
	println ("Bug reports and general questions should be sent to "
			 "<" PACKAGE_BUGREPORT ">.");
}

static void
show_version (void)
{
	println ("OSN freehttpd version " PACKAGE_VERSION);
	println ("License: AGPLv3.0 or later, this is free software.");
}

int
main (int argc, char **argv)
{
	invocation_name = argv[0];

	while (true)
	{
		int long_index = 0;
		int opt = getopt_long (argc, argv, short_options, long_options,
							   &long_index);

		if (opt == -1)
			break;

		switch (opt)
		{
			case 'h':
				usage ();
				exit (EXIT_SUCCESS);
				break;

			case 'V':
				show_version ();
				exit (EXIT_SUCCESS);
				break;

			case '?':
			default:
				exit (EXIT_FAILURE);
				break;
		}
	}

	return start_master ();
}
