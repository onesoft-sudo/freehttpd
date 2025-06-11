#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "htable.h"

struct htable *
htable_create (size_t capacity)
{
    capacity = capacity > 0 ? capacity : HTABLE_DEFAULT_CAPACITY;

    struct htable *table = calloc (1, sizeof (struct htable));

    if (!table)
        return NULL;

    table->capacity = capacity;
    table->count = 0;
    table->head = NULL;
    table->tail = NULL;
    table->buckets = calloc (capacity, sizeof (struct htable_entry));

    if (!table->buckets)
    {
        free (table);
        return NULL;
    }

    return table;
}

void
htable_destroy (struct htable *table)
{
    if (!table)
        return;

    free (table->buckets);
    free (table);
}

static size_t
htable_hash_fnv1a (uint64_t key, size_t capacity)
{
    const uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325;
    const uint64_t FNV_PRIME = 0x100000001b3;

    uint64_t hash = FNV_OFFSET_BASIS;

    hash ^= key;
    hash *= FNV_PRIME;

    return hash % capacity;
}

void *
htable_get (struct htable *table, uint64_t key)
{
    key = key == 0 ? UINT64_MAX : key;

    size_t hash = htable_hash_fnv1a (key, table->capacity);
    struct htable_entry *entry = &table->buckets[hash];
    bool first_iteration = true;

    while (entry)
    {
        if (entry->key == key)
            return entry->data;

        if (!first_iteration && !entry->next)
            break;

        entry = entry->next;

        if (entry == NULL)
        {
            entry = table->head;
            first_iteration = false;
        }
    }

    return NULL;
}

bool
htable_set (struct htable *table, uint64_t key, void *data)
{
    if (table->count >= ((table->capacity * 75) / 100)
        && !htable_resize (table, table->capacity >= 1024 * 1024
                                      ? table->capacity + 1024 * 1024
                                      : table->capacity * 2))
    {
#ifndef NDEBUG
        size_t newcap = table->capacity >= 1024 * 1024
                            ? table->capacity + 1024 * 1024
                            : table->capacity * 2;
        fprintf (stderr,
                 "%s: Failed to resize hash table for key %" PRIu64 "\n",
                 __func__, key);
        fprintf (stderr,
                 "Current capacity: %zu, count: %zu, new capacity: %zu\n",
                 table->capacity, table->count, newcap);
#endif

        return false;
    }

    key = key == 0 ? UINT64_MAX : key;

    size_t hash = htable_hash_fnv1a (key, table->capacity);

    for (; hash < table->capacity; hash++)
    {
        struct htable_entry *entry = &table->buckets[hash];

        if (entry->key == key)
        {
            entry->data = data;
            return true;
        }

        if (entry->key == 0)
        {
            entry->key = key;
            entry->data = data;

            entry->next = NULL;
            entry->prev = table->tail;

            if (table->tail)
                table->tail->next = entry;
            else
                table->head = entry;

            table->tail = entry;
            table->count++;

            return true;
        }
    }

#ifndef NDEBUG
    fprintf (stderr, "%s: Hash table is full, cannot insert key %" PRIu64 "\n",
             __func__, key);
#endif

    return false;
}

void *
htable_remove (struct htable *table, uint64_t key)
{
    if (table->count == 0)
        return NULL;

    key = key == 0 ? UINT64_MAX : key;

    size_t hash = htable_hash_fnv1a (key, table->capacity);
    struct htable_entry *entry = &table->buckets[hash];
    bool first_iteration = true;

    while (entry)
    {
        if (entry->key == key)
        {
            void *data = entry->data;

            entry->key = 0;
            entry->data = NULL;

            if (entry->prev)
                entry->prev->next = entry->next;
            else
                table->head = entry->next;

            if (entry->next)
                entry->next->prev = entry->prev;
            else
                table->tail = entry->prev;

            entry->next = NULL;
            entry->prev = NULL;
            table->count--;

            return data;
        }

        if (!first_iteration && entry == &table->buckets[hash])
            break;

        entry = entry->next;

        if (entry == NULL)
        {
            entry = table->head;
            first_iteration = false;
        }
    }

    return NULL;
}

bool
htable_resize (struct htable *table, size_t new_capacity)
{
    if (new_capacity <= table->capacity)
    {
        return false;
    }

    struct htable_entry *new_buckets
        = calloc (new_capacity, sizeof (struct htable_entry));

    if (!new_buckets)
    {
        return false;
    }

    struct htable_entry *head = table->head;
    struct htable_entry *new_head = NULL, *new_tail = NULL;

    while (head)
    {
        size_t new_hash = htable_hash_fnv1a (head->key, new_capacity);

        for (; new_hash < new_capacity; new_hash++)
        {
            struct htable_entry *entry = &new_buckets[new_hash];

            if (entry->key == 0)
            {
                entry->key = head->key;
                entry->data = head->data;

                entry->next = NULL;
                entry->prev = new_tail;

                if (new_tail)
                    new_tail->next = entry;
                else
                    new_head = entry;

                new_tail = entry;

#ifndef NDEBUG
                printf ("Moved key %" PRIu64 " to new bucket %zu\n", head->key,
                        new_hash);
#endif
                break;
            }
        }

        head = head->next;
    }

    free (table->buckets);

    table->head = new_head;
    table->tail = new_tail;
    table->buckets = new_buckets;
    table->capacity = new_capacity;

    return true;
}

bool
htable_contains (struct htable *table, uint64_t key)
{
    if (!table || table->count == 0)
        return false;

    key = key == 0 ? UINT64_MAX : key;

    size_t hash = htable_hash_fnv1a (key, table->capacity);
    struct htable_entry *entry = &table->buckets[hash];
    bool first_iteration = true;

    while (entry)
    {
        if (entry->key == key)
            return true;

        if (!first_iteration && !entry->next)
            break;

        entry = entry->next;

        if (entry == NULL)
        {
            entry = table->head;
            first_iteration = false;
        }
    }

    return false;
}