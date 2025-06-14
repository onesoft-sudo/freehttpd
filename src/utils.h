#ifndef FHTTPD_UTILS_H
#define FHTTPD_UTILS_H

#include <stdint.h>

uint64_t get_current_timestamp (void);
_Noreturn void freeze (void);
char *str_trim_whitespace (const char *str, size_t len, size_t *out_len);

#endif /* FHTTPD_UTILS_H */