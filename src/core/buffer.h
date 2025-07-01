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

#ifndef FH_BUFFER_H
#define FH_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#include "types.h"

enum fh_buf_type
{
	FH_BUF_DATA,
	FH_BUF_FILE
};

struct fh_buf_file
{
	fd_t fd;
	off_t start, end;
};

struct fh_buf_data
{
	uint8_t *start;
	size_t len;
	bool is_readonly;
};

union fh_buf_payload
{
	struct fh_buf_data data;
	struct fh_buf_file file;
};

struct fh_buf
{
	enum fh_buf_type type;
	union fh_buf_payload payload;
};

void fh_buf_dump (const struct fh_buf *buf);

#endif /* FH_BUFFER_H */
