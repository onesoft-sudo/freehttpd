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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strutils.h"

char *
str_trim_whitespace (const char *str, size_t len, size_t *out_len)
{
	if (len == 0)
	{
		*out_len = 0;
		return strdup ("");
	}

	size_t start = 0, end = len - 1;

	while (start < len && isspace (str[start]))
		start++;

	while (end > 0 && isspace (str[end]))
		end--;

	if (start > end)
	{
		*out_len = 0;
		return strdup ("");
	}

	size_t trimmed_len = end - start + 1;
	char *trimmed_str = malloc (trimmed_len + 1);

	if (!trimmed_str)
	{
		*out_len = 0;
		return NULL;
	}

	memcpy (trimmed_str, str + start, trimmed_len);
	trimmed_str[trimmed_len] = 0;
	*out_len = trimmed_len;

	return trimmed_str;
}

struct str_split_result *
str_split (const char *haystack, const char *needle)
{
	char **strings = NULL;
	size_t count = 0, i = 0, j = 0, last_split_i = 0;

	while (haystack[i])
	{
		if (haystack[i] == needle[j])
		{
			size_t end = i;

			while (haystack[i] && needle[j] && haystack[i] == needle[j])
			{
				i++;
				j++;
			}

			if (!needle[j])
			{
				char *str = strndup (haystack + last_split_i, end - last_split_i);

				if (!str)
					goto str_split_err;

				char **new_strings = realloc (strings, sizeof (char *) * (count + 1));

				if (!new_strings)
				{
					free (str);
					goto str_split_err;
				}

				count++;
				strings = new_strings;
				strings[count - 1] = str;
				j = 0;
				last_split_i = i;
				continue;
			}

			j = 0;
			continue;
		}

		i++;
	}

	if (haystack[0])
	{
		char *str = strdup (haystack + last_split_i);

		if (!str)
			goto str_split_err;

		char **new_strings = realloc (strings, sizeof (char *) * (count + 1));

		if (!new_strings)
		{
			free (str);
			goto str_split_err;
		}

		strings = new_strings;
		strings[count++] = str;
	}

	struct str_split_result *result = malloc (sizeof (*result));

	if (!result)
		goto str_split_err;

	result->count = count;
	result->strings = strings;

	return result;

str_split_err:
	for (size_t i = 0; i < count; i++)
		free (strings[i]);

	free (strings);
	return NULL;
}

void
str_split_free (struct str_split_result *result)
{
	for (size_t i = 0; i < result->count; i++)
		free (result->strings[i]);

	free (result->strings);
	free (result);
}
