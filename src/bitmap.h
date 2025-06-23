#ifndef FHTTPD_BITMAP_H
#define FHTTPD_BITMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct bitmap
{
	uint64_t *bits;
	size_t size;
	size_t bit_size : 6;
};

typedef struct bitmap bitmap_t;

bitmap_t *bitmap_create (bitmap_t *bitmap);
void bitmap_free (bitmap_t *bitmap, bool in_heap);
bool bitmap_set (bitmap_t *bitmap, size_t pos, bool bit);
bool bitmap_get (bitmap_t *bitmap, size_t pos);
void bitmap_print (bitmap_t *bitmap);

#endif /* FHTTPD_BITMAP_H */
