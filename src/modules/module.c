#define _GNU_SOURCE

#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdio.h>

#define FH_LOG_MODULE_NAME "module"

#include "module.h"
#include "utils/path.h"
#include "log/log.h"

#ifdef HAVE_CONFPATHS_H
#include "confpaths.h"
#endif /* HAVE_CONFPATHS_H */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

struct fh_module_manager *
fh_module_manager_create (void)
{
    struct fh_module_manager *manager = calloc (1, sizeof (*manager));

    if (!manager)
        return NULL;

    return manager;
}

static bool
fh_module_manager_load_file (struct fh_module_manager *manager, const char *filepath)
{
    fh_pr_info ("Loading module: %s", filepath);
    return true;
}

bool
fh_module_manager_load (struct fh_module_manager *manager)
{
    const char module_path[] = FHTTPD_MODULE_PATH;
    const char moudle_ext[] = SHARED_LIBRARY_EXTENSION;
    struct dirent **entries;
    int entry_count = scandir (module_path, &entries, NULL, &versionsort);

    if (entry_count < 0)
        return false;

    for (int i = 0; i < entry_count; i++)
    {
        if (!strcmp (entries[i]->d_name, ".") || !strcmp (entries[i]->d_name, ".."))
            goto loop_continue;

        char *pos = strrchr (entries[i]->d_name, '.');

        if (strcmp (pos, "." SHARED_LIBRARY_EXTENSION))
            goto loop_continue;

        char module_so_path[PATH_MAX + 1] = {0};

        if (!path_join (module_so_path, module_path, sizeof (module_path) - 1, entries[i]->d_name, strlen(entries[i]->d_name), PATH_MAX))
            goto loop_continue;
        
        if (!fh_module_manager_load_file (manager, module_so_path))
            fh_pr_err ("Failed to load module: %s", module_so_path);

        loop_continue:
            free (entries[i]);
    }

    free (entries);
    return true;
}

void
fh_module_manager_free (struct fh_module_manager *manager)
{
    free (manager);
}