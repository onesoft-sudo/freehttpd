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

#include <stdlib.h>
#include <string.h>

#include "base64.h"

static const char base64_encode_char_index[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_decode_char_index[] = {
	['A'] = 0,	['B'] = 1,	['C'] = 2,	['D'] = 3,	['E'] = 4,	['F'] = 5,	['G'] = 6,	['H'] = 7,
	['I'] = 8,	['J'] = 9,	['K'] = 10, ['L'] = 11, ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15,
	['Q'] = 16, ['R'] = 17, ['S'] = 18, ['T'] = 19, ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
	['Y'] = 24, ['Z'] = 25, ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29, ['e'] = 30, ['f'] = 31,
	['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35, ['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39,
	['o'] = 40, ['p'] = 41, ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45, ['u'] = 46, ['v'] = 47,
	['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51, ['0'] = 52, ['1'] = 53, ['2'] = 54, ['3'] = 55,
	['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59, ['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63,
};

bool
fh_base64_encode (struct fh_base64_buf *b64, const char *data, size_t len)
{
	len = len ? len : strlen (data);
	size_t pad = 0;
	// b64->buf = fh_pool_alloc (pool, (len < 4 ? 4 : (((len * 4UL) / 3UL) + 3)) + 1);

	if (!b64->buf)
		return false;

	for (size_t i = 0; i < len;)
	{
		size_t rem = len - i;
		uint32_t bits;

		if (rem == 1)
		{
			bits = data[i] << 16U;
			i += 1;
		}
		else if (rem == 2)
		{
			bits = (data[i] << 16U) | (data[i + 1] << 8U);
			i += 2;
		}
		else if (rem > 2)
		{
			bits = (data[i] << 16U) | (data[i + 1] << 8U) | data[i + 2];
			i += 3;
		}

		b64->buf[b64->size++] = base64_encode_char_index[(bits >> 18U) & 0x3F];
		b64->buf[b64->size++] = base64_encode_char_index[(bits >> 12U) & 0x3F];

		if (rem >= 2)
			b64->buf[b64->size++] = base64_encode_char_index[(bits >> 6U) & 0x3F];

		if (rem >= 3)
			b64->buf[b64->size++] = base64_encode_char_index[bits & 0x3F];

		if (i >= len)
			pad = 3 - rem;
	}

	while (pad-- > 0)
		b64->buf[b64->size++] = '=';

	b64->buf[b64->size] = 0;
	return true;
}

bool
fh_base64_decode (struct fh_base64_buf *b64, const char *data, size_t len)
{
	len = len ? len : strlen (data);

	if (len < 4)
		return false;

	// b64->buf = fh_pool_alloc (pool, ((len * 3) / 4) + 1);

	if (!b64->buf)
		return false;

	for (size_t i = 0; i < len; i += 4)
	{
		if (len - i != 0 && len - i < 4)
			return false;

		uint32_t bits = (base64_decode_char_index[(uint8_t) data[i]] << 18U) | (base64_decode_char_index[(uint8_t) data[i + 1]] << 12U)
						| (base64_decode_char_index[(uint8_t) data[i + 2]] << 6U) | base64_decode_char_index[(uint8_t) data[i + 3]];

		b64->buf[b64->size++] = (bits >> 16U) & 0xFF;
		b64->buf[b64->size++] = (bits >> 8U) & 0xFF;
		b64->buf[b64->size++] = bits & 0xFF;
	}

	b64->buf[b64->size] = 0;
	return true;
}