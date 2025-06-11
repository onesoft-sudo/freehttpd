#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#undef NDEBUG

#include "htable.h"

int
main (void)
{
    struct htable *table = htable_create (0);

    assert (table != NULL);

    htable_set (table, 5, (void *) "test");

    assert (htable_get (table, 5) == (void *) "test");

    char data[500];

    const size_t count = 10000;

    for (int i = 0; i < count; i++)
    {
        printf ("Setting key %d\n", i);
        assert (htable_set (table, i, (void *) data + i) == true);
    }

    for (int i = 0; i < count; i++)
    {
        printf ("Getting key %d\n", i);
        assert (htable_get (table, i) == (void *) data + i);
        assert (htable_contains (table, i) == true);
    }

    for (int i = 0; i < count; i++)
    {
        if (i % 2 == 0)
        {
            printf ("Removing key %d\n", i);
            assert (htable_remove (table, i) == (void *) data + i);
        }
    }

    for (int i = 0; i < count; i++)
    {
        if (i % 2 == 0)
        {
            printf ("Checking key %d [even]\n", i);
            assert (htable_get (table, i) == NULL);
            assert (htable_contains (table, i) == false);
        }
        else
        {
            printf ("Checking key %d [odd]\n", i);
            assert (htable_get (table, i) == (void *) data + i);
            assert (htable_contains (table, i) == true);
        }
    }

    htable_destroy (table);
    return 0;
}