#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <stdbool.h>

#include "bitstream.h"

int
main (void)
{
    bitstream_t *bstream_heap = bitstream_create (NULL);
    assert (bstream_heap != NULL);
    bitstream_free (bstream_heap, true);

    bitstream_t bstream;
    assert (&bstream == bitstream_create (&bstream));

    assert (bitstream_get (&bstream, 4) == 0);
    assert (bitstream_get (&bstream, 0) == 0);
    assert (bitstream_get (&bstream, 10397363) == 0);
    assert (bitstream_get (&bstream, __SIZE_MAX__) == 0);

    for (size_t i = 0; i < 10000; i++)
    {
        if (i & 1)
            assert (bitstream_set (&bstream, i, true) == false);
        else
            assert (bitstream_set (&bstream, i, false) == false);
    }

    bitstream_print (&bstream);

    for (size_t i = 0; i < 10000; i++)
    {
        bool v = bitstream_get (&bstream, i);
        assert (v == (i & 1));
    }

    bitstream_free (&bstream, false);
    return 0;
}