#ifndef FHTTPD_ITABLE_H
#define FHTTPD_ITABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ITABLE_DEFAULT_CAPACITY 16

struct itable_entry
{
	uint64_t key;
	void *data;
	struct itable_entry *next;
	struct itable_entry *prev;
};

struct itable
{
	size_t capacity;
	size_t count;
	struct itable_entry *head;
	struct itable_entry *tail;
	struct itable_entry *buckets;
};

struct itable *itable_create (size_t capacity);
void itable_destroy (struct itable *table);
void *itable_get (struct itable *table, uint64_t key);
bool itable_set (struct itable *table, uint64_t key, void *data);
void *itable_remove (struct itable *table, uint64_t key);
bool itable_resize (struct itable *table, size_t new_capacity);
bool itable_contains (struct itable *table, uint64_t key);

#endif /* FHTTPD_ITABLE_H */
