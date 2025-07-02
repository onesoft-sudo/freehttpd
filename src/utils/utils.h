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

#ifndef FHTTPD_UTILS_H
#define FHTTPD_UTILS_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "compat.h"

#if EAGAIN == EWOULDBLOCK
#define would_block() (errno == EAGAIN)
#else
#define would_block() (errno == EAGAIN || errno == EWOULDBLOCK)
#endif

bool fd_set_nonblocking (int fd);
_noreturn void freeze (void);
bool path_normalize (char *dest, const char *src, size_t *len_ptr);
bool format_size (size_t size, char buf[64], size_t *num, char unit[3]);
const char *get_file_extension (const char *filename);

#endif /* FHTTPD_UTILS_H */
