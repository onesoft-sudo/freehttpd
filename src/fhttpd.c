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

#define FHTTPD_LOG_MODULE_NAME "main"

#include "conf.h"
#include "log.h"
#include "server.h"
#include "strutils.h"

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef FHTTPD_ENABLE_SYSTEMD
	#include <systemd/sd-daemon.h>
#endif /* FHTTPD_ENABLE_SYSTEMD */

int
main (int argc __attribute_maybe_unused__, char **argv __attribute_maybe_unused__)
{
	fhttpd_log_set_output (stderr, stdout);

	struct fhttpd_master *master = fhttpd_master_create ();

	if (!master)
	{
#ifdef FHTTPD_ENABLE_SYSTEMD
		sd_notifyf (0, "ERRNO=%d", errno);
#endif /* FHTTPD_ENABLE_SYSTEMD */

		fhttpd_log_error ("Failed to create server");
		return 1;
	}

	if (!fhttpd_master_prepare (master))
	{
#ifdef FHTTPD_ENABLE_SYSTEMD
		sd_notifyf (0, "ERRNO=%d", errno);
#endif /* FHTTPD_ENABLE_SYSTEMD */

		fhttpd_master_destroy (master);
		return 1;
	}

	if (!fhttpd_master_start (master))
	{
#ifdef FHTTPD_ENABLE_SYSTEMD
		sd_notifyf (0, "ERRNO=%d", errno);
#endif /* FHTTPD_ENABLE_SYSTEMD */

		fhttpd_log_error ("Failed to run server: %s", strerror (errno));
		fhttpd_master_destroy (master);
		return 1;
	}

	fhttpd_master_destroy (master);
	return 0;
}
