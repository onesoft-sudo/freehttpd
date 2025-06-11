#include <stdlib.h>
#include <string.h>

#include "list.h"

list_t *
list_create (size_t element_size)
{
    list_t *list = malloc (sizeof (list_t));

    if (!list)
    {
        return NULL;
    }

    list->data = malloc (element_size * 8);

    if (!list->data)
    {
        free (list);
        return NULL;
    }

    list->size = 0;
    list->capacity = 8;
    list->element_size = element_size;

    return list;
}

void
list_destroy (list_t *list)
{
    if (!list)
        return;

    free (list->data);
    free (list);
}

bool
list_add (list_t *list, void *element)
{
    if (list->size >= list->capacity)
    {
        size_t new_capacity = list->capacity * 2;

        void *new_data
            = realloc (list->data, new_capacity * list->element_size);

        if (!new_data)
            return false;

        list->data = new_data;
        list->capacity = new_capacity;
    }

    memcpy ((char *) list->data + (list->size * list->element_size), element,
            list->element_size);

    list->size++;
    return true;
}

void *
list_get (list_t *list, size_t index, int *flags)
{
    if (index >= list->size)
    {
        if (flags)
            *flags |= LIST_NO_ENTRY;

        return NULL;
    }

    if (flags)
        *flags &= ~LIST_NO_ENTRY;

    return ((char *) list->data) + (index * list->element_size);
}

bool
list_remove (list_t *list, size_t index, int *flags)
{
    if (index >= list->size)
    {
        if (flags)
            *flags |= LIST_NO_ENTRY;

        return false;
    }

    if (flags)
        *flags &= ~LIST_NO_ENTRY;

    size_t offset = index * list->element_size;
    size_t next_offset = (index + 1) * list->element_size;

    for (size_t i = next_offset; i < list->size * list->element_size; i++)
    {
        ((char *) list->data)[offset++] = ((char *) list->data)[i];
    }

    return true;
}