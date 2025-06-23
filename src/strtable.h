#ifndef FHTTPD_STRTABLE_H
#define FHTTPD_STRTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STRTABLE_DEFAULT_CAPACITY 16

struct strtable_entry
{
	char *key;
	void *data;
	struct strtable_entry *next;
	struct strtable_entry *prev;
};

struct strtable
{
	size_t capacity;
	size_t count;
	struct strtable_entry *head;
	struct strtable_entry *tail;
	struct strtable_entry *buckets;
};

struct strtable *strtable_create (size_t capacity);
void strtable_destroy (struct strtable *table);
void *strtable_get (struct strtable *table, const char *key);
bool strtable_set (struct strtable *table, const char *key, void *data);
void *strtable_remove (struct strtable *table, const char *key);
bool strtable_resize (struct strtable *table, size_t new_capacity);
bool strtable_contains (struct strtable *table, const char *key);

#endif /* FHTTPD_STRTABLE_H */
