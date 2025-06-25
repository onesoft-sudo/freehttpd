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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
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


uint64_t
get_current_timestamp (void)
{
	struct timespec now;
	clock_gettime (CLOCK_REALTIME, &now);
	return (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
}

_noreturn void
freeze (void)
{
	fprintf (stderr, "Alert: process %d will be frozen\n", getpid ());
	fflush (stderr);

	while (true)
		pause ();
}

bool
path_normalize (char *dest, const char *src, size_t *len_ptr)
{
	size_t len = *len_ptr;

	if (!dest || !src || len == 0 || len > PATH_MAX)
		return false;

	char buffer[len + 1];
	size_t i = 0, j = 0;

	while (i < len && j < len)
	{
		if (src[i] == 0)
			break;

		if (src[i] == '/')
		{
			if (j == 0 || buffer[j - 1] != '/')
				buffer[j++] = '/';

			i++;
			continue;
		}

		if (src[i] == '.')
		{
			if (i + 1 >= len || src[i + 1] == '/')
			{
				i += 2;
				continue;
			}
			else if (i + 1 < len && src[i + 1] == '.' && (i + 2 >= len || src[i + 2] == '/'))
			{
				if (j > 0)
				{
					j--;

					while (j > 0 && buffer[j - 1] != '/')
						j--;
				}

				i += 2;

				continue;
			}
		}

		if (src[i] == '/' || src[i] == '\\')
		{
			i++;
			continue;
		}

		if (i < len && j < len)
			buffer[j++] = src[i++];
	}

	if (j == 0 || j > PATH_MAX || j > len)
		return false;

	if (j > 1 && buffer[j - 1] == '/')
		j--;

	buffer[j] = 0;
	memcpy (dest, buffer, j);
	dest[j] = 0;
	*len_ptr = j;

	return true;
}

bool
format_size (size_t size, char buf[64], size_t *num, char unit[3])
{
	if (!buf && !num && !unit)
		return false;

	const char *units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };
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
		snprintf (buf, 64, "%zu%s", size, units[unit_index]);

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
