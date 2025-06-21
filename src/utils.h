#ifndef FHTTPD_UTILS_H
#define FHTTPD_UTILS_H

#include <stdint.h>
#include <errno.h>
#include <stdbool.h>

#include "strutils.h"

#if EAGAIN == EWOULDBLOCK
#define would_block() (errno == EAGAIN)
#else
#define would_block() (errno == EAGAIN || errno == EWOULDBLOCK)
#endif

uint64_t get_current_timestamp (void);
_Noreturn void freeze (void);
char *str_trim_whitespace (const char *str, size_t len, size_t *out_len);
bool path_normalize (char *dest, const char *src, size_t *len_ptr);
bool format_size (size_t size, char buf[64], size_t *num, char unit[3]);
const char *get_file_extension (const char *filename);

#endif /* FHTTPD_UTILS_H */