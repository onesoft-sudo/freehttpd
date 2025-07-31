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

#ifndef FH_H2_H
#define FH_H2_H

#include <stdint.h>
#include <stddef.h>

enum h2_frame_type
{
	H2_FRAME_DATA = 0x0,
	H2_FRAME_HEADERS,
	H2_FRAME_PRIORITY,
	H2_FRAME_RST_STREAM,
	H2_FRAME_SETTINGS,
	H2_FRAME_PUSH_PROMISE,
	H2_FRAME_PING,
	H2_FRAME_GOAWAY,
	H2_FRAME_WINDOW_UPDATE,
	H2_FRAME_CONTINUATION,
};

struct h2_frame_header
{
	uint8_t length[3];
	uint8_t type;
	uint8_t flags;
	uint8_t r_stream_id[4];
};

enum h2_settings_type
{
	SETTINGS_HEADER_TABLE_SIZE = 0x0,
	SETTINGS_ENABLE_PUSH,
	SETTINGS_MAX_CONCURRENT_STREAMS,
	SETTINGS_INITIAL_WINDOW_SIZE,
	SETTINGS_MAX_FRAME_SIZE,
	SETTINGS_MAX_HEADER_LIST_SIZE
};

struct h2_settings
{
	size_t header_table_size;
	size_t initial_window_size;
	size_t max_frame_size;
	size_t max_header_list_size;
	bool enable_push;
	uint8_t max_concurrent_streams;
};

struct h2_stream
{
	size_t id;
};

struct fh_h2_ctx
{
	struct h2_stream *stream_head;
	struct h2_stream *stream_tail;
};

#endif /* FH_H2_H */
