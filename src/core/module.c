#define _GNU_SOURCE

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define FH_LOG_MODULE_NAME "module"

#include "log/log.h"
#include "module.h"
#include "utils/path.h"

#ifdef HAVE_CONFPATHS_H
	#include "confpaths.h"
#endif /* HAVE_CONFPATHS_H */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif /* HAVE_CONFIG_H */

struct fh_module_internal
{
	struct fh_module pub_module;
	const struct fh_module_info *info;
	dlhandle_t handle;
	struct fh_module_internal *prev;
	struct fh_module_internal *next;
};

struct fh_module_manager *
fh_module_manager_create (void)
{
	struct fh_module_manager *manager = calloc (1, sizeof (*manager));

	if (!manager)
		return NULL;

	return manager;
}

static bool
fh_module_manager_load_file (struct fh_module_manager *manager,
							 const char *filepath)
{
	fh_pr_info ("Loading module: %s", filepath);

	dlhandle_t handle = dlopen (filepath, RTLD_NOW);

	if (!handle)
	{
		fh_pr_err ("Module load error: %s: dlopen failed: %s", filepath, strerror (errno));
		return false;
	}

	const struct fh_module_info *info_ptr = (const struct fh_module_info *) dlsym (handle, "module_info");

	if (!info_ptr)
	{
		fh_pr_err ("Module load error: %s: Module does not export a 'module_info' symbol", filepath);
		dlclose (handle);
		return false;
	}

	if (info_ptr->signature != MODULE_SIGNATURE)
	{
		dlclose (handle);
		fh_pr_err ("Module load error: %s: Module has invalid signature, perhaps it is not a module?", filepath);
		return false;
	}

	struct fh_module_internal *module = calloc (1, sizeof (*module));

	if (!module)
	{
		fh_pr_err ("Module load error: %s: Memory allocation failed: %s", filepath, strerror (errno));
		dlclose (handle);
		return false;
	}

	module->handle = handle;
	module->info = info_ptr;

	int rc;

	if ((rc = module->info->on_load (&module->pub_module)) != FH_OK)
	{
		fh_pr_err ("Module %s returned initialization error: %d", module->info->name, rc);
		free (module);
		dlclose (handle);
		return false;
	}

	if (!manager->loaded_module_head)
	{
		manager->loaded_module_head = manager->loaded_module_tail = module;
		manager->loaded_module_count = 1;
	}
	else
	{
		manager->loaded_module_tail->next = module;
		module->prev = manager->loaded_module_tail;
		manager->loaded_module_tail = module;
	}

	fh_pr_info ("Loaded module: %s", module->info->name);
	return true;
}

bool
fh_module_manager_load (struct fh_module_manager *manager)
{
	const char module_path[] = FHTTPD_MODULE_PATH;
	struct dirent **entries;
	int entry_count = scandir (module_path, &entries, NULL, &versionsort);

	if (entry_count < 0)
		return false;

	for (int i = 0; i < entry_count; i++)
	{
		if (!strcmp (entries[i]->d_name, ".")
			|| !strcmp (entries[i]->d_name, ".."))
			goto loop_continue;

		char *pos = strrchr (entries[i]->d_name, '.');

		if (strcmp (pos, "." SHARED_LIBRARY_EXTENSION))
			goto loop_continue;

		char module_so_path[PATH_MAX + 1] = { 0 };

		if (!path_join (module_so_path, module_path, sizeof (module_path) - 1,
						entries[i]->d_name, strlen (entries[i]->d_name),
						PATH_MAX))
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
	for (struct fh_module_internal *module = manager->loaded_module_head; module; )
	{
		int rc = module->info->on_unload (&module->pub_module);

		if (rc != FH_OK)
		{
			fh_pr_err ("Module %s returned unload error: %d", module->info->name, rc);
		}

		struct fh_module_internal *next = module->next;

		dlclose (module->handle);
		free (module);

		module = next;
	}

	free (manager);
}
