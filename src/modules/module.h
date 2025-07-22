#ifndef FH_MODULE_H
#define FH_MODULE_H

#include "core/server.h"

#define __module_scope __attribute__((section(".rodata")))
#define MODULE_SIGNATURE 0x043D20FAU

enum fh_module_errno
{
    FH_OK = 0,
    FH_UNKNOWN = 1,
    FH_NOMEM,
    FH_INTERNAL,
    FH_FATAL
};

enum fh_module_type
{
    FH_MODULE_GENERIC,
};

struct fh_module
{
    struct fh_server *server;
};

struct fh_module_internal;

typedef int (*fh_module_on_load_cb_t)(const struct fh_module *);
typedef int (*fh_module_on_unload_cb_t)(const struct fh_module *);

struct fh_module_info
{
	uint32_t signature;
	enum fh_module_type type;
	const char *name;
    fh_module_on_load_cb_t on_load;
    fh_module_on_unload_cb_t on_unload;
};

struct fh_module_manager
{
    struct fh_module_internal *loaded_module_head;
    struct fh_module_internal *loaded_module_tail;
	size_t loaded_module_count;
};

typedef void * dlhandle_t;

struct fh_module_manager *fh_module_manager_create (void);
bool fh_module_manager_load (struct fh_module_manager *manager);
void fh_module_manager_free (struct fh_module_manager *manager);

#endif /* FH_MODULE_H */
