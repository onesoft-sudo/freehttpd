#ifndef FH_UTILS_PATH_H
#define FH_UTILS_PATH_H

#include <stdlib.h>
#include <stdbool.h>

bool path_normalize (char *dest, const char *src, size_t *len_ptr);
bool path_join (char *dest, const char *src1, size_t src1_len, const char *src2, size_t src2_len, size_t max_len);

#endif /* FH_UTILS_PATH_H */