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