#ifndef FH_BASE64_H
#define FH_BASE64_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct fh_base64_buf
{
    char *buf;
    size_t size;
};

bool fh_base64_encode (struct fh_base64_buf *b64_buf, const char *data, size_t len);
bool fh_base64_decode (struct fh_base64_buf *b64_buf, const char *data, size_t len);
void fh_base64_free (struct fh_base64_buf *b64_buf);

#endif /* FH_BASE64_H */