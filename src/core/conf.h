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

#ifndef FHTTPD_CONF_H
#define FHTTPD_CONF_H

#include <stdbool.h>
#include <stdint.h>

#include "log/log.h"

enum conf_parser_error
{
	CONF_PARSER_ERROR_NONE = 0,
	CONF_PARSER_ERROR_SYNTAX_ERROR,
	CONF_PARSER_ERROR_MEMORY,
	CONF_PARSER_ERROR_INVALID_CONFIG,
};

struct fh_bound_addr
{
	char *hostname;
	size_t hostname_len;
	char *full_hostname;
	size_t full_hostname_len;
	uint16_t port;
};

struct fh_host_config
{
	struct fh_bound_addr *bound_addrs;
	size_t bound_addr_count;
	struct fh_config *host_config;
};

struct fh_config
{
	char *conf_root;
	size_t worker_count;

	enum fh_log_level logging_min_level;
	char *logging_file;
	char *logging_error_file;

	struct fh_host_config *hosts;
	size_t host_count;
	ssize_t default_host_index;

	char *docroot;

	size_t sec_max_response_body_size;
	size_t sec_max_connections;
	uint32_t sec_recv_timeout;
	uint32_t sec_send_timeout;
	uint32_t sec_header_timeout;
	uint32_t sec_body_timeout;
};

struct fhttpd_conf_parser;

struct fhttpd_conf_parser *fhttpd_conf_parser_create (const char *filename);
int fhttpd_conf_parser_read (struct fhttpd_conf_parser *parser);
void fhttpd_conf_parser_destroy (struct fhttpd_conf_parser *parser);
bool fhttpd_conf_parser_print_error (struct fhttpd_conf_parser *parser);
enum conf_parser_error fhttpd_conf_parser_last_error (struct fhttpd_conf_parser *parser);
const char *fhttpd_conf_parser_strerror (enum conf_parser_error error);
struct fh_config *fhttpd_conf_process (struct fhttpd_conf_parser *parser);
void fhttpd_conf_print_config (const struct fh_config *config, int indent);
void fhttpd_conf_free_config (struct fh_config *config);

#endif /* FHTTPD_CONF_H */
