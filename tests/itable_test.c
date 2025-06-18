#undef NDEBUG

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "itable.h"

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
