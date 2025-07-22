#ifndef FH_MODULE_H
#define FH_MODULE_H

#include "core/server.h"

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

typedef int (*fh_module_on_load_cb_t)(const struct fh_module *);
typedef int (*fh_module_on_unload_cb_t)(const struct fh_module *);

struct fh_module_info
{
    enum fh_module_type type;
    fh_module_on_load_cb_t on_load;
    fh_module_on_unload_cb_t on_unload;
};

struct fh_module_manager
{
    struct fh_module **loaded_modules;
    size_t loaded_module_count;
};

typedef void * dlhandle_t;

struct fh_module_manager *fh_module_manager_create (void);
bool fh_module_manager_load (struct fh_module_manager *manager);
void fh_module_manager_free (struct fh_module_manager *manager);

#endif /* FH_MODULE_H */