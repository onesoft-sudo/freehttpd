#ifndef FHTTPD_HTABLE_H
#define FHTTPD_HTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HTABLE_DEFAULT_CAPACITY 16

struct htable_entry
{
    uint64_t key;
    void *data;
    struct htable_entry *next;
    struct htable_entry *prev;
};

struct htable
{
    size_t capacity;
    size_t count;
    struct htable_entry *head;
    struct htable_entry *tail;
    struct htable_entry *buckets;
};

struct htable *htable_create (size_t capacity);
void htable_destroy (struct htable *table);
void *htable_get (struct htable *table, uint64_t key);
bool htable_set (struct htable *table, uint64_t key, void *data);
void *htable_remove (struct htable *table, uint64_t key);
bool htable_resize (struct htable *table, size_t new_capacity);
bool htable_contains (struct htable *table, uint64_t key);

#endif /* FHTTPD_HTABLE_H */