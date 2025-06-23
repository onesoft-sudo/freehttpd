#ifndef FHTTPD_STRUTILS_H
#define FHTTPD_STRUTILS_H

#include <stddef.h>

struct str_split_result
{
	char **strings;
	size_t count;
};

char *str_trim_whitespace (const char *str, size_t len, size_t *out_len);
struct str_split_result *str_split (const char *haystack, const char *needle);
void str_split_free (struct str_split_result *result);

#endif /* FHTTPD_STRUTILS_H */
