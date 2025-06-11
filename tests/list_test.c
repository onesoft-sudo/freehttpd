#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#undef NDEBUG

#include "list.h"

int
main (void)
{
    list_t *list = list_create_type (int);
    assert (list != NULL);
    assert (list->size == 0);
    assert (list->capacity == 8);
    assert (list->element_size == sizeof (int));
    assert (list->data != NULL);
    int value = 42;
    assert (list_add (list, &value) == true);
    assert (list->size == 1);
    assert (*(int *) list->data == value);
    int flags = 0;
    int *retrieved_value = (int *) list_get (list, 0, &flags);
    assert (retrieved_value != NULL);
    assert (*retrieved_value == value);
    assert (flags == 0);

    list_destroy (list);
    list = list_create_type (int);

    int numbers[4096];

    for (int i = 0; i < 4096; i++)
    {
        numbers[i] = i * 2;
        assert (list_add (list, &numbers[i]) == true);
    }

    for (int i = 0; i < 4096; i++)
    {
        assert (*(int *) list_get (list, i, NULL) == i * 2);
    }

    list_destroy (list);
    return 0;
}