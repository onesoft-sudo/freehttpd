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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FHTTPD_LOG_MODULE_NAME "http1"

#include "core/conn.h"
#include "http1.h"
#include "log/log.h"
#include "utils/utils.h"
#include "mm/pool.h"

#ifdef HAVE_RESOURCES
	#include "resources.h"
#endif

#define HTTP1_PARSER_NEXT 0
#define HTTP1_PARSER_RETURN(value) ((1 << 8) | (value))

static const char *http1_method_names[] = {
	[FHTTPD_METHOD_GET] = "GET",	   [FHTTPD_METHOD_POST] = "POST",		[FHTTPD_METHOD_PUT] = "PUT",
	[FHTTPD_METHOD_DELETE] = "DELETE", [FHTTPD_METHOD_HEAD] = "HEAD",		[FHTTPD_METHOD_OPTIONS] = "OPTIONS",
	[FHTTPD_METHOD_PATCH] = "PATCH",   [FHTTPD_METHOD_CONNECT] = "CONNECT", [FHTTPD_METHOD_TRACE] = "TRACE",
};

static const size_t http1_method_names_count = sizeof (http1_method_names) / sizeof (http1_method_names[0]);

void
http1_parser_ctx_free (struct http1_parser_ctx *ctx)
{

}

void
http1_response_ctx_free (struct http1_response_ctx *ctx)
{
	
}