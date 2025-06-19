#ifndef FHTTPD_AUTOINDEX_H
#define FHTTPD_AUTOINDEX_H

#include <stdbool.h>
#include <stddef.h>

#include "protocol.h"

bool fhttpd_autoindex (const struct fhttpd_request *request, struct fhttpd_response *response, const char *filepath,
                                  size_t filepath_len __attribute_maybe_unused__);


#endif /* FHTTPD_AUTOINDEX_H */