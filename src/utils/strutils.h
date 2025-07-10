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

#ifndef FHTTPD_STRUTILS_H
#define FHTTPD_STRUTILS_H

#include <stddef.h>

struct str_split_result
{
	char **strings;
	size_t count;
};

const char *str_trim_whitespace (const char *str, size_t len, size_t *out_len);
struct str_split_result *str_split (const char *haystack, const char *needle);
void str_split_free (struct str_split_result *result);
uint64_t strntoull (const char *str, size_t len, int base);

#endif /* FHTTPD_STRUTILS_H */
