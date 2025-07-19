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

#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "path.h"
#include "utils.h"

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
path_join (char *dest, const char *src1, size_t src1_len, const char *src2, size_t src2_len, size_t max_len)
{
    if (!src1_len)
        src1_len = strlen (src1);

    size_t wrote = MIN (src1_len, max_len);
    strncpy (dest, src1, wrote);

    if (wrote >= max_len - 1)
        return false;

    dest[wrote++] = '/';

    if (!src2_len)
        src2_len = strlen (src2);

    if (wrote + src2_len > max_len)
        src2_len = max_len - wrote;

    strncpy (dest + wrote, src2, src2_len);
    wrote += src2_len;

    return true;
}
