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

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <stdbool.h>

#include "bitmap.h"

int
main (void)
{
    bitmap_t *bstream_heap = bitmap_create (NULL);
    assert (bstream_heap != NULL);
    bitmap_free (bstream_heap, true);

    bitmap_t bstream;
    assert (&bstream == bitmap_create (&bstream));

    assert (bitmap_get (&bstream, 4) == 0);
    assert (bitmap_get (&bstream, 0) == 0);
    assert (bitmap_get (&bstream, 10397363) == 0);
    assert (bitmap_get (&bstream, __SIZE_MAX__) == 0);

    for (size_t i = 0; i < 10000; i++)
    {
        if (i & 1)
            assert (bitmap_set (&bstream, i, true) == false);
        else
            assert (bitmap_set (&bstream, i, false) == false);
    }

    bitmap_print (&bstream);

    for (size_t i = 0; i < 10000; i++)
    {
        bool v = bitmap_get (&bstream, i);
        assert (v == (i & 1));
    }

    bitmap_free (&bstream, false);
    return 0;
}
