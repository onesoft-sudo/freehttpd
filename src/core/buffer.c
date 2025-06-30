#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"

void
fh_buf_dump (const struct fh_buf *buf)
{
    printf ("Buffer <%p>:", (void *) buf);
    printf ("Type: %s\n", buf->type == FH_BUF_DATA ? "DATA" : buf->type == FH_BUF_FILE ? "FILE" : "???");
    
    switch (buf->type)
    {
        case FH_BUF_DATA:
            printf ("Size: %zu\n", buf->payload.data.len);
            printf ("Readonly: %u\n", buf->payload.data.is_readonly);
            printf ("Data: |%.*s|\n", (int) buf->payload.data.len, buf->payload.data.start);
            break;

        case FH_BUF_FILE:
            printf ("FD: %d\n", buf->payload.file.fd);
            printf ("Range: [%zu-%zu]\n", buf->payload.file.start, buf->payload.file.end);
    }

    printf ("\n");
}