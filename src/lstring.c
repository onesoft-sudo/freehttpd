#include "lstring.h"

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>

#define LSTRING_MAGIC 0x4C53544DU

struct lstring_internal
{
    uint32_t magic;
    size_t len;
    size_t cap;
};

lstring_t lstring_create(const char *str, size_t len)
{
    if (!str)
        return NULL;

    len = len == 0 ? strlen(str) : len;
    struct lstring_internal *lstr_info = malloc(sizeof(struct lstring_internal) + len + 1);

    if (!lstr_info)
        return NULL;

    lstr_info->len = len;
    lstr_info->cap = len;
    lstr_info->magic = LSTRING_MAGIC;

    lstring_t lstr = (lstring_t) (lstr_info + 1);
    memcpy(lstr, str, len);
    lstr[len] = '\0';

    return lstr;
}

void lstring_destroy(lstring_t lstr)
{
    if (!lstr)
        return;

    struct lstring_internal *lstr_info = (struct lstring_internal *) lstr - 1;

    assert (lstr_info->magic == LSTRING_MAGIC && "Invalid lstring pointer");
    free(lstr_info);
}

size_t lstring_length(lstring_t lstr)
{
    if (!lstr)
        return 0;

    struct lstring_internal *lstr_info = (struct lstring_internal *) lstr - 1;

    assert (lstr_info->magic == LSTRING_MAGIC && "Invalid lstring pointer");
    return lstr_info->len;
}

bool lstring_append(lstring_t *lstr, const char *str, size_t len)
{
    if (!lstr || !*lstr || !str)
        return false;

    struct lstring_internal *lstr_info = ((struct lstring_internal *) *lstr) - 1;

    assert (lstr_info->magic == LSTRING_MAGIC && "Invalid lstring pointer");
    len = len == 0 ? strlen(str) : len;

    size_t new_cap = lstr_info->len + len + 128;
    size_t new_len = lstr_info->len + len;

    if (new_cap >= lstr_info->cap) {
        struct lstring_internal *new_lstr_info = realloc(lstr_info, sizeof(struct lstring_internal) + new_cap + 1);

        if (!new_lstr_info)
            return false;

        new_lstr_info->cap = new_cap;
        lstr_info = new_lstr_info;
    }

    *lstr = (lstring_t) (lstr_info + 1);

    memcpy(*lstr + lstr_info->len, str, len);
    (*lstr)[new_len] = 0;
    lstr_info->len = new_len;

    return true;
}

bool lstring_trim(lstring_t *lstr)
{
    if (!lstr || !*lstr)
        return false;

    struct lstring_internal *lstr_info = ((struct lstring_internal *) *lstr) - 1;

    assert (lstr_info->magic == LSTRING_MAGIC && "Invalid lstring pointer");

    size_t len = lstr_info->len;

    if (len == 0)
        return true;

    size_t start = 0, end = len - 1;

    while (start < len && isspace((*lstr)[start]))
        start++;

    while (end > start && isspace((*lstr)[end]))
        end--;

    size_t new_len = end - start + 1;

    if (new_len == 0)
    {
        lstr_info->len = 0;
        return true;
    }

    if (new_len == len)
        return true;
    
    memmove(*lstr, *lstr + start, new_len);
    (*lstr)[new_len] = 0;
    lstr_info->len = new_len;
    
    return true;
}