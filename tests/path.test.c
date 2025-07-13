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

#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/path.h"

int
main (void)
{
    const char *paths[]
        = { "/home/user/../test/./file.txt", "/home/user/../../test/file.txt", "/home/user/test/.././file.txt",
            "/home/user/test/./file.txt", "/home/user/test/file.txt", "/home/user/test/../file.txt",
            "/home/user/test/../../file.txt", "/home/user/test/../../../file.txt",
            "/home/user/test/../../../../file.txt", "/home/user/test/../../../..//file.txt",
            "/home/user/test/../../../../..//file.txt", "/home/user/test/../../../../../file.txt",
            "/home/user/test/../../../../../..//file.txt", "/home/user/test/../../../../../../../file.txt",
            "/home/user/test/../../../../../../../..//file.txt", "/home/user/test/../../../../../../../../../file.txt",
            "/home/user/test/../../../../../../../../../..//file.txt",
            "/home/user/test/../../../../../../../../../../../file.txt",

            /* Complex cases */
            "/home/user/./test/../test2/./file.txt", "/home/user/./test/../../test2/./file.txt",
            "/home/user/./test/../test2/../../file.txt", "/home/user/./test/../test2/../../../file.txt",
            "/home/user/./test/../test2/../../../../file.txt", "/home/user/./test/../test2/../../../../../file.txt",
            "/home/user/./test/../test2/../../../../../../file.txt",
            "/home/user/./test/../test2/../../../../../../../file.txt",
            "/home/user/./test/../test2/../../../../../../../../../file.txt",
            "/home/user/./test/../test2/../../../../../../../../../../../file.txt",

            "/../user/./test/../test2/./file.txt", "/a/../README/../.././../.././../../../.././" };

    const char *expected_results[] = { "/home/test/file.txt",
                                       "/test/file.txt",
                                       "/home/user/file.txt",
                                       "/home/user/test/file.txt",
                                       "/home/user/test/file.txt",
                                       "/home/user/file.txt",
                                       "/home/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/home/user/test2/file.txt",
                                       "/home/test2/file.txt",
                                       "/home/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/file.txt",
                                       "/user/test2/file.txt",
                                       "/" };

    char normalized_path[PATH_MAX + 1];
    size_t len = 0;

    for (size_t i = 0; i < sizeof (paths) / sizeof (paths[0]); i++)
    {
        len = PATH_MAX;
        assert (path_normalize (normalized_path, paths[i], &len));
        printf ("[%zu] Normalized: '%s' => '%s' [Expected '%s']\n", i, paths[i], normalized_path, expected_results[i]);
        assert (normalized_path[len] == 0);
        assert (strcmp (normalized_path, expected_results[i]) == 0);
    }

    return 0;
}
