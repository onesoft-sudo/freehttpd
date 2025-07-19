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

#ifndef FH_UTILS_PATH_H
#define FH_UTILS_PATH_H

#include <stdlib.h>
#include <stdbool.h>

bool path_normalize (char *dest, const char *src, size_t *len_ptr);
bool path_join (char *dest, const char *src1, size_t src1_len, const char *src2, size_t src2_len, size_t max_len);

#endif /* FH_UTILS_PATH_H */
