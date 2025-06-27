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
