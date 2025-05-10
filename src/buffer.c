#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include "buffer.h"

struct buffer *buffer_create(size_t initial_capacity)
{
    struct buffer *buf = malloc(sizeof(struct buffer));

    if (!buf)
        return NULL;

    buf->data = malloc(initial_capacity);

    if (!buf->data)
    {
        free(buf);
        return NULL;
    }

    buf->length = 0;
    buf->capacity = initial_capacity;

    return buf;
}

void buffer_destroy(struct buffer *buf)
{
    if (!buf)
        return;

    free(buf->data);
    free(buf);
}

bool buffer_resize(struct buffer *buf, size_t new_capacity)
{
    if (!buf || new_capacity <= buf->capacity)
        return false;

    uint8_t *new_data = realloc(buf->data, new_capacity);

    if (!new_data)
        return false;

    buf->data = new_data;
    buf->capacity = new_capacity;
    return true;
}

bool buffer_append(struct buffer *buf, const uint8_t *data, size_t length)
{
    if (!buf || !data || length == 0)
        return false;

    if (buf->length + length > buf->capacity)
    {
        if (!buffer_resize(buf, buf->length + length))
            return false;
    }

    memcpy(buf->data + buf->length, data, length);
    buf->length += length;
    return true;
}

bool buffer_aprintf(struct buffer *buf, const char *format, ...)
{
    if (!buf || !format)
        return false;

    va_list args;
    va_start(args, format);

    char *temp_buf = NULL;
    vasprintf(&temp_buf, format, args);
    va_end(args);

    if (!temp_buf)
        return false;

    size_t length = strlen(temp_buf);
    buffer_append(buf, (uint8_t *) temp_buf, length);
    free(temp_buf);
    
    return true;
}

bool buffer_clear(struct buffer *buf)
{
    if (!buf)
        return false;

    buf->length = 0;
    return true;
}