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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"

int
main (int argc, char **argv)
{
	struct fh_base64_buf b64 = { 0 };

	if (argv[1][0] == 'e')
		assert (fh_base64_encode (&b64, argv[2], 0) == true);
	else
		assert (fh_base64_decode (&b64, argv[2], 0) == true);

	printf ("%s\n", b64.buf);
	fflush (stdout);
	free (b64.buf);
	return 0;
}
