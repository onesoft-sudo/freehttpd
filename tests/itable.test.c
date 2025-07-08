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
#include <stdio.h>
#include <stdlib.h>

#include "hash/itable.h"

int
main (void)
{
    struct itable *table = itable_create (0);

    assert (table != NULL);

    itable_set (table, 5, (void *) "test");

    assert (itable_get (table, 5) == (void *) "test");

    char data[500];

    const size_t count = 10000;

    for (int i = 0; i < count; i++)
    {
        printf ("Setting key %d\n", i);
        assert (itable_set (table, i, (void *) data + i) == true);
    }

    for (int i = 0; i < count; i++)
    {
        printf ("Getting key %d\n", i);
        assert (itable_get (table, i) == (void *) data + i);
        assert (itable_contains (table, i) == true);
    }

    for (int i = 0; i < count; i++)
    {
        if (i % 2 == 0)
        {
            printf ("Removing key %d\n", i);
            assert (itable_remove (table, i) == (void *) data + i);
        }
    }

    for (int i = 0; i < count; i++)
    {
        if (i % 2 == 0)
        {
            printf ("Checking key %d [even]\n", i);
            assert (itable_get (table, i) == NULL);
            assert (itable_contains (table, i) == false);
        }
        else
        {
            printf ("Checking key %d [odd]\n", i);
            assert (itable_get (table, i) == (void *) data + i);
            assert (itable_contains (table, i) == true);
        }
    }

    itable_destroy (table);
    return 0;
}
