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

#include "hash/strtable.h"
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

struct fh_config_logging
{
	bool enabled;
	enum fh_log_level min_level;
	char *file;
	char *error_file;
};

struct fh_config_host
{
	struct fh_bound_addr addr;
	char *docroot;
	bool is_default;
	struct fh_config_logging *logging;
};

struct fh_config_security
{
	size_t max_response_body_size;
	size_t max_connections;
	uint32_t recv_timeout;
	uint32_t send_timeout;
	uint32_t header_timeout;
	uint32_t body_timeout;
};

struct fh_config
{
	char *conf_root;
	size_t worker_count;
	/* (const char *host) => (struct fh_config_host *host_config) */
	struct strtable *hosts;
	struct fh_config_host *default_host_config;
	bool no_free_hosts : 1;
	struct fh_config_logging *logging;
	struct fh_config_security *security;
};

enum conf_token_type
{
	CONF_TOKEN_INVALID = 0,
	CONF_TOKEN_STRING,
	CONF_TOKEN_INT,
	CONF_TOKEN_FLOAT,
	CONF_TOKEN_BOOLEAN,
	CONF_TOKEN_IDENTIFIER,
	CONF_TOKEN_OPEN_BRACKET,
	CONF_TOKEN_CLOSE_BRACKET,
	CONF_TOKEN_OPEN_BRACE,
	CONF_TOKEN_CLOSE_BRACE,
	CONF_TOKEN_OPEN_PARENTHESIS,
	CONF_TOKEN_CLOSE_PARENTHESIS,
	CONF_TOKEN_COMMA,
	CONF_TOKEN_SEMICOLON,
	CONF_TOKEN_EQUALS,
	CONF_TOKEN_COLON,
	CONF_TOKEN_NULL,
	CONF_TOKEN_INCLUDE,
	CONF_TOKEN_INCLUDE_OPTIONAL,
	CONF_TOKEN_EOF
};

struct conf_token
{
	enum conf_token_type type;
	char *value;
	size_t length;
	size_t line;
	size_t column;
};

enum conf_node_type
{
	CONF_NODE_INVALID = 0,
	CONF_NODE_ROOT,
	CONF_NODE_ASSIGNMENT,
	CONF_NODE_BLOCK,
	CONF_NODE_ARRAY,
	CONF_NODE_IDENTIFIER,
	CONF_NODE_LITERAL,
	CONF_NODE_FUNCTION_CALL,
	CONF_NODE_INCLUDE
};

enum conf_literal_kind
{
	CONF_LITERAL_INT,
	CONF_LITERAL_FLOAT,
	CONF_LITERAL_STRING,
	CONF_LITERAL_BOOLEAN
};

struct conf_node
{
	enum conf_node_type type;
	struct conf_node *parent;
	size_t line;
	size_t column;

	union
	{
		struct
		{
			enum conf_literal_kind kind;

			union
			{
				int64_t int_value;
				double float_value;
				bool bool_value;

				struct
				{
					char *value;
					size_t length;
				} str;
			} value;
		} literal;

		struct
		{
			char *value;
			size_t identifier_length;
		} identifier;

		struct
		{
			struct conf_node *name;
			struct conf_node **children;
			size_t child_count;
			struct conf_node **args;
			size_t argc;
		} block;

		struct
		{
			struct conf_node **children;
			size_t child_count;
		} root;

		struct
		{
			struct conf_node *left;
			struct conf_node *right;
		} assignment;

		struct
		{
			struct conf_node **elements;
			size_t element_count;
		} array;

		struct
		{
			char *function_name;
			size_t function_name_length;
			struct conf_node **args;
			size_t argc;
		} function_call;

		struct
		{
			char *filename;
			bool optional;
		} include;
	} details;
};

struct fh_conf_parser
{
	char *filename;
	char *source;
	size_t source_len;

	struct conf_token *tokens;
	size_t token_count, token_index;

	char *last_error;
	enum conf_parser_error last_error_code;
	size_t error_line, error_column;
	char *error_filename;

	char **include_fv;
	size_t include_fc;

	bool root_parser;
};

struct fh_traverse_ctx;

struct fh_conf_parser *fh_conf_parser_create (const char *filename);
int fh_conf_parser_read (struct fh_conf_parser *parser);
void fh_conf_parser_destroy (struct fh_conf_parser *parser);
bool fh_conf_parser_print_error (struct fh_conf_parser *parser);
enum conf_parser_error fh_conf_parser_last_error (struct fh_conf_parser *parser);
const char *fh_conf_parser_strerror (enum conf_parser_error error);
struct fh_config *fh_conf_process (struct fh_conf_parser *parser, struct fh_traverse_ctx *ctx, struct fh_config *config);
int fh_conf_parser_error (struct fh_conf_parser *parser,
						  enum conf_parser_error code, size_t line,
						  size_t column, const char *message, ...);
void
fh_conf_print_node (const struct conf_node *node, int indent);

#ifdef CORE_CONF_IMPLEMENTATION

struct fh_conf_parser *fh_conf_parser_create_internal (const char *filename, bool is_root);

#endif

#endif /* FHTTPD_CONF_H */
