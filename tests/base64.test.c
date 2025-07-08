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

#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "digest/base64.h"

int
main (int argc, char **argv)
{
	size_t len = strlen (argv[2]);
	bool encode = argv[1][0] == 'e';
	struct fh_base64_buf b64 = { .buf = malloc (encode ? ((len / 3) * 4) + 5 : (((len / 4) * 3) + 5)), .size = 0 };

	if (encode)
		assert (fh_base64_encode (&b64, argv[2], len) == true);
	else
		assert (fh_base64_decode (&b64, argv[2], len) == true);

	printf ("%s\n", b64.buf);
	fflush (stdout);
	free (b64.buf);
	return 0;
}
