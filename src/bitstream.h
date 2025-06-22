#ifndef FHTTPD_BITSTREAM_H
#define FHTTPD_BITSTREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct bitstream {
    uint64_t *bits;
    size_t size;
    size_t bit_size : 6;
};

typedef struct bitstream bitstream_t;

bitstream_t *bitstream_create (bitstream_t *dest);
void bitstream_free (bitstream_t *stream, bool in_heap);
bool bitstream_set (bitstream_t *stream, size_t pos, bool bit);
bool bitstream_get (bitstream_t *stream, size_t pos);
void bitstream_print (bitstream_t *stream);

#endif /* FHTTPD_BITSTREAM_H */