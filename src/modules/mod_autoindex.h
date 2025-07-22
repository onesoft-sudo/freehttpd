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

#ifndef FH_MODULES_MOD_AUTOINDEX_H
#define FH_MODULES_MOD_AUTOINDEX_H

#include <stddef.h>
#include <stdint.h>

#include "core/conn.h"
#include "core/stream.h"
#include "router/filesystem.h"
#include "http/http1_request.h"
#include "http/http1_response.h"
#include "router/router.h"
#include "utils/utils.h"

struct fh_autoindex
{
	struct fh_router *router;
	struct fh_conn *conn;
	const struct fh_request *request;
	struct fh_response *response;
	const char *filename;
	size_t filename_len;
	const struct stat64 *st;
};

bool fh_autoindex_handle (struct fh_autoindex *autoindex);

#endif /* FH_MODULES_MOD_AUTOINDEX_H */
