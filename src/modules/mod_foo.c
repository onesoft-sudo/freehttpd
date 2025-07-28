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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "foo"

#include "core/server.h"
#include "core/module.h"
#include "log/log.h"

static int
module_load (const struct fh_module *module)
{
	(void) module;
	fh_pr_info ("Hello world from mod_foo!");
	return FH_OK;
}

static int
module_unload (const struct fh_module *module)
{
	(void) module;
	fh_pr_info ("Goodbye world from mod_foo!");
	return FH_OK;
}

struct fh_module_info module_info = {
	.signature = MODULE_SIGNATURE,
	.type = FH_MODULE_GENERIC,
	.name = "mod_foo",
	.on_load = &module_load,
	.on_unload = &module_unload,
};
