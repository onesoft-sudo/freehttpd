#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

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