#ifndef FHTTPD_LIST_H
#define FHTTPD_LIST_H

#include <stdbool.h>
#include <stddef.h>

#define LIST_NO_ENTRY 0x01

struct list
{
    void *data;
    size_t size;
    size_t capacity;
    size_t element_size;
};

typedef struct list list_t;

list_t *list_create (size_t element_size);
void list_destroy (list_t *list);
bool list_add (list_t *list, void *element);
void *list_get (list_t *list, size_t index, int *flags);
bool list_remove (list_t *list, size_t index, int *flags);

#define list_create_type(type) list_create (sizeof (type))

#endif /* FHTTPD_LIST_H */