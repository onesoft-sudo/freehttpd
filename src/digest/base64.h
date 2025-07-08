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

#ifndef FH_BASE64_H
#define FH_BASE64_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "mm/pool.h"

struct fh_base64_buf
{
    char *buf;
    size_t size;
};

bool fh_base64_encode (struct fh_base64_buf *b64_buf, const char *data, size_t len);
bool fh_base64_decode (struct fh_base64_buf *b64_buf, const char *data, size_t len);

#endif /* FH_BASE64_H */
