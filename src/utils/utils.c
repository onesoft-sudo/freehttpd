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

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "compat.h"
#include "utils.h"

bool
fd_set_nonblocking (int fd)
{
	int flags = fcntl (fd, F_GETFL);

	if (flags < 0)
		return false;

	if (fcntl (fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return false;

	return true;
}

_noreturn void
freeze (void)
{
	fprintf (stderr, "ALERT: process %d will be frozen\n", getpid ());
	fflush (stderr);

	struct sigaction act;

	act.sa_flags = 0;
	act.sa_handler = SIG_DFL;
	sigemptyset (&act.sa_mask);

	sigaction (SIGINT, &act, NULL);
	sigaction (SIGTERM, &act, NULL);

	while (true)
		pause ();
}

bool
format_size (size_t size, char buf[64], size_t *num, char unit[3],
			 size_t *out_len)
{
	if (!buf && !num && !unit)
		return false;

	const char *units[]
		= { "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };
	size_t unit_index = 0;

	while (size >= 1024 && unit_index < sizeof (units) / sizeof (units[0]) - 1)
	{
		size /= 1024;
		unit_index++;
	}

	if (num)
		*num = size;

	if (unit)
		strncpy (unit, units[unit_index], 2);

	if (buf)
	{
		int rc = snprintf (buf, 64, "%zu%s", size, units[unit_index]);

		if (rc < 0)
			return false;

		*out_len = (size_t) rc;
	}

	return true;
}

const char *
get_file_extension (const char *filename)
{
	if (!filename)
		return NULL;

	const char *dot = strrchr (filename, '.');

	if (!dot || dot == filename)
		return NULL;

	return dot + 1;
}
