#ifndef FREEHTTPD_BUFFER_H
#define FREEHTTPD_BUFFER_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

struct buffer
{
    uint8_t *data;
    size_t length;
    size_t capacity;
};

struct buffer *buffer_create(size_t initial_capacity);
void buffer_destroy(struct buffer *buf);
bool buffer_append(struct buffer *buf, const uint8_t *data, size_t length);
bool buffer_resize(struct buffer *buf, size_t new_capacity);
bool buffer_clear(struct buffer *buf);
bool buffer_aprintf(struct buffer *buf, const char *format, ...) __attribute__((format(printf, 2, 3)));

#endif /* FREEHTTPD_BUFFER_H */