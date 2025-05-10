#ifndef FREEHTTPD_LSTRING_H
#define FREEHTTPD_LSTRING_H

#include <stddef.h>
#include <stdbool.h>

typedef char * lstring_t;

#define LSTR(s) ((lstring_t) (lstring_create((s), 0)))

lstring_t lstring_create(const char *str, size_t len);
void lstring_destroy(lstring_t lstr);
bool lstring_append(lstring_t *lstr, const char *str, size_t len);
size_t lstring_length(lstring_t lstr);
bool lstring_trim(lstring_t *lstr);

#endif /* FREEHTTPD_LSTRING_H */