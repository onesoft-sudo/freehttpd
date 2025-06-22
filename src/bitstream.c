#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitstream.h"

bitstream_t *
bitstream_create (bitstream_t *stream)
{
	if (!stream)
		stream = malloc (sizeof (*stream));

	if (!stream)
		return NULL;

	stream->size = 0;
	stream->bit_size = 0;
	stream->bits = NULL;

	return stream;
}

void
bitstream_free (bitstream_t *stream, bool in_heap)
{
	free (stream->bits);

	if (in_heap)
		free (stream);
}

bool
bitstream_get (bitstream_t *stream, size_t pos)
{
	size_t i = pos >> 6;

	if (i >= stream->size)
		return false;

	uint64_t bits = stream->bits[i];
	uint64_t offset = pos & 0x3F;

	return bits & (0x1ULL << (0x3F - offset));
}

bool
bitstream_set (bitstream_t *stream, size_t pos, bool bit)
{
	size_t i = pos >> 6;

	if (i >= stream->size)
	{
		uint64_t *bits = realloc (stream->bits, sizeof (uint64_t) * (i + 1));

		if (!bits)
			return false;

		memset (bits + stream->size, 0, sizeof (uint64_t) * (i + 1 - stream->size));
		stream->bits = bits;
		stream->size = i + 1;
		stream->bit_size = 0;
	}

	bit &= 0x1;

	uint64_t bits = stream->bits[i];
	uint64_t offset = pos & 0x3F;
	bool old_value = bits & (0x1ULL << (0x3F - offset));

	if (offset >= stream->bit_size)
		stream->bit_size = offset + 1;

	if (bit)
		bits |= (0x1ULL) << (0x3F - offset);
	else
		bits &= ~((0x1ULL) << (0x3F - offset));

	stream->bits[i] = bits;
	return old_value;
}

void
bitstream_print (bitstream_t *stream)
{
	printf ("<bitstream [%zu bits] ",
			stream->size == 0 ? 0 : (((stream->size - 1) * 64) + (stream->bit_size == 0 ? 64 : stream->bit_size)));

	for (size_t i = 0; i < stream->size; i++)
	{
		uint64_t bits = stream->bits[i];
		size_t bit_size = i == (stream->size - 1) ? stream->bit_size == 0 ? 0x40 : stream->bit_size : 0x40;

		for (size_t j = 0; j < bit_size; j++)
			fputc (bits & (1ULL << (0x3F - j)) ? '1' : '0', stdout);
	}

	printf (">\n");
}