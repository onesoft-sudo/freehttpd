#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "foo"

#include "core/server.h"
#include "log/log.h"
#include "module.h"

static int
module_load (const struct fh_module *module)
{
	fh_pr_info ("Hello world from mod_foo!");
	return FH_OK;
}

static int
module_unload (const struct fh_module *module)
{
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
