/*
 * This file is part of OSN freehttpd.
 * 
 * Copyright (C) 2025  OSN Developers.
 *
 * OSN freehttpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * OSN freehttpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.
 */

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
