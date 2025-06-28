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

#ifndef FHTTPD_LOOP_H
#define FHTTPD_LOOP_H

enum loop_operation
{
	LOOP_OPERATION_NONE,
	LOOP_OPERATION_CONTINUE,
	LOOP_OPERATION_BREAK
};

typedef enum loop_operation loop_op_t;

#define LOOP_OPERATION(op)                                                                                             \
	if ((op) == LOOP_OPERATION_BREAK)                                                                                  \
		break;                                                                                                         \
	else if ((op) == LOOP_OPERATION_CONTINUE)                                                                          \
		continue;

#endif /* FHTTPD_LOOP_H */
