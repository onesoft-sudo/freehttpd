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

#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif /* HAVE_CONFIG_H */

#define CORE_CONF_IMPLEMENTATION

#include "conf.h"
#include "confproc.h"
#include "utils/strutils.h"

struct fh_conf_parser *
fh_conf_parser_create_internal (const char *filename, bool is_root)
{
	struct fh_conf_parser *parser = calloc (1, sizeof (*parser));

	if (!parser)
		return NULL;

	parser->filename = realpath (filename, NULL);

	if (!parser->filename)
	{
		free (parser);
		return NULL;
	}

	parser->error_line = 0;
	parser->error_column = 0;
	parser->root_parser = is_root;

	if (is_root)
	{
		parser->include_fc = 1;
		parser->include_fv = malloc (sizeof (char *));

		if (!parser->include_fv)
		{
			free (parser->filename);
			free (parser);
			return NULL;
		}

		parser->include_fv[0] = strdup (parser->filename);
	}

	return parser;
}

struct fh_conf_parser *
fh_conf_parser_create (const char *filename)
{
	return fh_conf_parser_create_internal (filename, true);
}

int
fh_conf_parser_read (struct fh_conf_parser *parser)
{
	errno = 0;

	FILE *file = fopen (parser->filename, "r");

	if (!file)
		return errno;

	if (fseek (file, 0, SEEK_END) < 0)
	{
		int err = errno;
		fclose (file);
		return err;
	}

	off_t file_size = ftell (file);

	if (file_size < 0)
	{
		int err = errno;
		fclose (file);
		return err;
	}

	if (fseek (file, 0, SEEK_SET) < 0)
	{
		int err = errno;
		fclose (file);
		return err;
	}

	parser->source = malloc (file_size + 1);

	if (!parser->source)
	{
		int err = errno;
		fclose (file);
		return err;
	}

	while (parser->source_len < (size_t) file_size && !feof (file))
	{
		size_t bytes_read
			= fread (parser->source + parser->source_len, 1,
					 (size_t) file_size - parser->source_len, file);

		if (bytes_read == 0)
			break;

		parser->source_len += bytes_read;
	}

	if (ferror (file) || !feof (file)
		|| parser->source_len != (size_t) file_size)
	{
		int err = errno;
		fclose (file);
		return err;
	}

	parser->source[parser->source_len] = 0;
	fclose (file);

	parser->tokens = NULL;
	parser->token_count = 0;
	parser->last_error = NULL;

	return 0;
}

static void
fh_conf_parser_free_token (struct conf_token *token)
{
	free (token->value);
}

int
fh_conf_parser_error (struct fh_conf_parser *parser,
						  enum conf_parser_error code, size_t line,
						  size_t column, const char *message, ...)
{
	if (parser->last_error)
		free (parser->last_error);

	parser->last_error = NULL;
	parser->last_error_code = code;
	parser->error_line = line;
	parser->error_column = column;

	va_list args;
	va_start (args, message);
	int rc = vasprintf (&parser->last_error, message, args);
	va_end (args);
	return rc;
}

static enum conf_parser_error
fh_conf_parser_add_token (struct fh_conf_parser *parser, size_t line,
							  size_t column, enum conf_token_type type,
							  const char *value, size_t length)
{
	struct conf_token *tokens = realloc (
		parser->tokens, sizeof (struct conf_token) * (parser->token_count + 1));

	if (!tokens)
	{
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, line,
								  column,
								  "Failed to allocate memory for tokens");
		return CONF_PARSER_ERROR_MEMORY;
	}

	parser->tokens = tokens;
	parser->tokens[parser->token_count].type = type;
	parser->tokens[parser->token_count].value = strndup (value, length);
	parser->tokens[parser->token_count].length = length;
	parser->tokens[parser->token_count].line = line;
	parser->tokens[parser->token_count].column = column;
	parser->token_count++;

	return CONF_PARSER_ERROR_NONE;
}

void
fh_conf_parser_destroy (struct fh_conf_parser *parser)
{
	if (!parser)
		return;

	if (parser->tokens)
	{
		for (size_t i = 0; i < parser->token_count; i++)
		{
			fh_conf_parser_free_token (&parser->tokens[i]);
		}

		free (parser->tokens);
	}

	if (parser->root_parser)
	{
		if (parser->include_fv)
		{
			for (size_t i = 0; i < parser->include_fc; i++)
				free (parser->include_fv[i]);

			free (parser->include_fv);
		}
	}

	free (parser->filename);
	free (parser->error_filename);
	free (parser->source);
	free (parser->last_error);
	free (parser);
}

static enum conf_parser_error
fh_conf_parser_tokenize (struct fh_conf_parser *parser)
{
	size_t i = 0;
	int rc;
	size_t line = 1, column = 1;

	while (i < parser->source_len)
	{
		char c = parser->source[i];

		if (isspace (c))
		{
			if (c == '\n')
			{
				line++;
				column = 1;
			}
			else
			{
				column++;
			}

			i++;
			continue;
		}

		if (c == '#')
		{
			while (i < parser->source_len && parser->source[i] != '\n')
			{
				i++;
				column++;
			}

			if (i < parser->source_len && parser->source[i] == '\n')
			{
				line++;
				column = 1;
				i++;
			}

			continue;
		}

		enum conf_token_type token_type = CONF_TOKEN_INVALID;

		switch (c)
		{
			case '{':
				token_type = CONF_TOKEN_OPEN_BRACE;
				break;

			case '}':
				token_type = CONF_TOKEN_CLOSE_BRACE;
				break;

			case '(':
				token_type = CONF_TOKEN_OPEN_PARENTHESIS;
				break;

			case ')':
				token_type = CONF_TOKEN_CLOSE_PARENTHESIS;
				break;

			case '[':
				token_type = CONF_TOKEN_OPEN_BRACKET;
				break;

			case ']':
				token_type = CONF_TOKEN_CLOSE_BRACKET;
				break;

			case ',':
				token_type = CONF_TOKEN_COMMA;
				break;

			case ';':
				token_type = CONF_TOKEN_SEMICOLON;
				break;

			case '=':
				token_type = CONF_TOKEN_EQUALS;
				break;

			default:
				break;
		}

		if (token_type != CONF_TOKEN_INVALID)
		{
			if ((rc = fh_conf_parser_add_token (parser, line, column,
													token_type, &c, 1))
				!= CONF_PARSER_ERROR_NONE)
				return rc;

			column++;
			i++;
			continue;
		}

		if (c == '"' || c == '\"')
		{
			char quote = c;
			size_t start = i + 1;
			size_t start_line = line, start_column = column;
			i++;
			column++;

			while (i < parser->source_len && parser->source[i] != '"'
				   && parser->source[i] != '\"')
			{
				if (parser->source[i] == '\n')
				{
					line++;
					column = 1;
				}
				else
				{
					column++;
				}

				i++;
			}

			if (i >= parser->source_len || parser->source[i] != quote)
			{
				fh_conf_parser_error (
					parser, CONF_PARSER_ERROR_SYNTAX_ERROR, line, column,
					"Unterminated string literal");
				return CONF_PARSER_ERROR_SYNTAX_ERROR;
			}

			size_t length = i - start;

			if ((rc = fh_conf_parser_add_token (
					 parser, start_line, start_column, CONF_TOKEN_STRING,
					 &parser->source[start], length))
				!= CONF_PARSER_ERROR_NONE)
				return rc;

			column++;
			i++;
			continue;
		}

		if (isdigit (c)
			|| (c == '-' && i + 1 < parser->source_len
				&& isdigit (parser->source[i + 1])))
		{
			size_t start = i;
			bool is_float = false;
			size_t start_line = line, start_column = column;

			while (i < parser->source_len
				   && (isdigit (parser->source[i]) || parser->source[i] == '.'
					   || parser->source[i] == '-'))
			{
				if (parser->source[i] == '\n')
				{
					line++;
					column = 1;
				}
				else
				{
					column++;
				}

				switch (parser->source[i])
				{
					case '.':
						if (is_float)
						{
							fh_conf_parser_error (
								parser, CONF_PARSER_ERROR_SYNTAX_ERROR, line,
								column, "Multiple decimal points in number");
							return CONF_PARSER_ERROR_SYNTAX_ERROR;
						}

						is_float = true;
						break;

					case '-':
						if (i != start)
						{
							fh_conf_parser_error (
								parser, CONF_PARSER_ERROR_SYNTAX_ERROR, line,
								column, "Unexpected '-' in number");
							return CONF_PARSER_ERROR_SYNTAX_ERROR;
						}

						break;

					default:
						break;
				}

				i++;
			}

			size_t length = i - start;

			if ((rc = fh_conf_parser_add_token (
					 parser, start_line, start_column,
					 is_float ? CONF_TOKEN_FLOAT : CONF_TOKEN_INT,
					 &parser->source[start], length))
				!= CONF_PARSER_ERROR_NONE)
				return rc;

			continue;
		}

		if (isalpha (c) || c == '_')
		{
			size_t start = i;
			size_t start_line = line, start_column = column;

			while (i < parser->source_len
				   && (isalnum (parser->source[i]) || parser->source[i] == '_'))
			{
				if (parser->source[i] == '\n')
				{
					line++;
					column = 1;
				}
				else
				{
					column++;
				}

				i++;
			}

			size_t length = i - start;
			enum conf_token_type token_type = CONF_TOKEN_IDENTIFIER;

			if (strncmp (&parser->source[start], "true", length) == 0
				|| strncmp (&parser->source[start], "yes", length) == 0)
				token_type = CONF_TOKEN_BOOLEAN;
			else if (strncmp (&parser->source[start], "false", length) == 0
					 || strncmp (&parser->source[start], "no", length) == 0)
				token_type = CONF_TOKEN_BOOLEAN;
			else if (strncmp (&parser->source[start], "null", length) == 0)
				token_type = CONF_TOKEN_NULL;
			else if (strncmp (&parser->source[start], "include", length) == 0)
				token_type = CONF_TOKEN_INCLUDE;
			else if (strncmp (&parser->source[start], "include_optional",
							  length)
					 == 0)
				token_type = CONF_TOKEN_INCLUDE_OPTIONAL;

			if ((rc = fh_conf_parser_add_token (
					 parser, start_line, start_column, token_type,
					 &parser->source[start], length))
				!= CONF_PARSER_ERROR_NONE)
				return rc;

			continue;
		}

		fh_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, line,
								  column, "Unexpected character '%c'", c);
		return CONF_PARSER_ERROR_SYNTAX_ERROR;
	}

	if ((rc = fh_conf_parser_add_token (parser, line, column,
											CONF_TOKEN_EOF, "[EOF]", 5))
		!= CONF_PARSER_ERROR_NONE)
		return rc;

	return CONF_PARSER_ERROR_NONE;
}

static void __attribute_maybe_unused__
fh_conf_parser_print_tokens (struct fh_conf_parser *parser)
{
	for (size_t i = 0; i < parser->token_count; i++)
	{
		struct conf_token *token = &parser->tokens[i];

		printf ("[%zu]: Token <type=%d, value='%.*s', line=%zu, column=%zu>\n",
				i, token->type, (int) token->length, token->value, token->line,
				token->column);
	}
}

enum conf_parser_error
fh_conf_parser_last_error (struct fh_conf_parser *parser)
{
	return parser->last_error_code;
}

bool
fh_conf_parser_print_error (struct fh_conf_parser *parser)
{
	if (parser->last_error)
	{
#ifdef FHTTPD_ENABLE_SYSTEMD
		fh_pr_err (
			"%s:%zu:%zu: %s\n",
			parser->error_filename ? parser->error_filename : parser->filename,
			parser->error_line, parser->error_column, parser->last_error);
#else  /* not FHTTPD_ENABLE_SYSTEMD */
		fprintf (stderr, "%s:%zu:%zu: %s\n",
				 parser->error_filename ? parser->error_filename
										: parser->filename,
				 parser->error_line, parser->error_column, parser->last_error);
#endif /* FHTTPD_ENABLE_SYSTEMD */
	}

	return parser->last_error != NULL;
}

const char *
fh_conf_parser_strerror (enum conf_parser_error error)
{
	switch (error)
	{
		case CONF_PARSER_ERROR_NONE:
			return "No error";
		case CONF_PARSER_ERROR_SYNTAX_ERROR:
			return "Syntax error in configuration file";
		case CONF_PARSER_ERROR_MEMORY:
			return "Memory allocation error";
		case CONF_PARSER_ERROR_INVALID_CONFIG:
			return "Invalid configuration";
		default:
			return "Unknown error";
	}
}

__attribute_maybe_unused__ static const char *
fh_conf_strtoken (enum conf_token_type type)
{
	switch (type)
	{
		case CONF_TOKEN_INVALID:
			return "INVALID";
		case CONF_TOKEN_STRING:
			return "STRING";
		case CONF_TOKEN_INT:
			return "INT";
		case CONF_TOKEN_FLOAT:
			return "FLOAT";
		case CONF_TOKEN_BOOLEAN:
			return "BOOLEAN";
		case CONF_TOKEN_IDENTIFIER:
			return "IDENTIFIER";
		case CONF_TOKEN_OPEN_BRACE:
			return "OPEN_BRACE";
		case CONF_TOKEN_CLOSE_BRACE:
			return "CLOSE_BRACE";
		case CONF_TOKEN_OPEN_PARENTHESIS:
			return "OPEN_PARENTHESIS";
		case CONF_TOKEN_CLOSE_PARENTHESIS:
			return "CLOSE_PARENTHESIS";
		case CONF_TOKEN_COMMA:
			return "COMMA";
		case CONF_TOKEN_SEMICOLON:
			return "SEMICOLON";
		case CONF_TOKEN_EQUALS:
			return "EQUALS";
		case CONF_TOKEN_COLON:
			return "COLON";
		case CONF_TOKEN_EOF:
			return "EOF";
		case CONF_TOKEN_OPEN_BRACKET:
			return "OPEN_BRACKET";
		case CONF_TOKEN_CLOSE_BRACKET:
			return "CLOSE_BRACKET";
		case CONF_TOKEN_NULL:
			return "NULL";
		case CONF_TOKEN_INCLUDE:
			return "INCLUDE";
		case CONF_TOKEN_INCLUDE_OPTIONAL:
			return "INCLUDE_OPTIONAL";
		default:
			return "UNKNOWN";
	}
}

static struct conf_node *
fh_conf_new_node (enum conf_node_type type, size_t line, size_t column)
{
	struct conf_node *node = calloc (1, sizeof (*node));

	if (!node)
		return NULL;

	node->type = type;
	node->line = line;
	node->column = column;

	return node;
}

static void
fh_conf_free_node (struct conf_node *node)
{
	if (!node)
		return;

	switch (node->type)
	{
		case CONF_NODE_ROOT:
			for (size_t i = 0; i < node->details.root.child_count; i++)
			{
				fh_conf_free_node (node->details.root.children[i]);
			}

			free (node->details.root.children);
			break;

		case CONF_NODE_ASSIGNMENT:
			fh_conf_free_node (node->details.assignment.left);
			fh_conf_free_node (node->details.assignment.right);
			break;

		case CONF_NODE_BLOCK:
			for (size_t i = 0; i < node->details.block.child_count; i++)
			{
				fh_conf_free_node (node->details.block.children[i]);
			}

			for (size_t i = 0; i < node->details.block.argc; i++)
			{
				fh_conf_free_node (node->details.block.args[i]);
			}

			fh_conf_free_node (node->details.block.name);
			free (node->details.block.args);
			free (node->details.block.children);
			break;

		case CONF_NODE_ARRAY:
			for (size_t i = 0; i < node->details.array.element_count; i++)
			{
				fh_conf_free_node (node->details.array.elements[i]);
			}

			free (node->details.array.elements);
			break;

		case CONF_NODE_IDENTIFIER:
			free (node->details.identifier.value);
			break;

		case CONF_NODE_LITERAL:
			if (node->details.literal.kind == CONF_LITERAL_STRING)
				free (node->details.literal.value.str.value);

			break;

		case CONF_NODE_FUNCTION_CALL:
			free (node->details.function_call.function_name);

			for (size_t i = 0; i < node->details.function_call.argc; i++)
			{
				fh_conf_free_node (node->details.function_call.args[i]);
			}

			free (node->details.function_call.args);
			break;

		case CONF_NODE_INCLUDE:
			free (node->details.include.filename);
			break;

		default:
			break;
	}

	free (node);
}

static inline bool
fh_conf_is_eof (struct fh_conf_parser *parser)
{
	return parser->token_index >= parser->token_count
		   || parser->tokens[parser->token_index].type == CONF_TOKEN_EOF;
}

static inline struct conf_token *
fh_conf_peek_token (struct fh_conf_parser *parser, size_t offset)
{
	if (parser->token_index + offset >= parser->token_count)
		return NULL;

	return &parser->tokens[parser->token_index + offset];
}

static inline struct conf_token *
fh_conf_consume_token (struct fh_conf_parser *parser)
{
	if (parser->token_index >= parser->token_count)
		return NULL;

	return &parser->tokens[parser->token_index++];
}

static struct conf_token *
fh_conf_expect_token (struct fh_conf_parser *parser,
						  enum conf_token_type expected_type)
{
	if (fh_conf_is_eof (parser))
	{
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR,
								  fh_conf_peek_token (parser, 0)->line,
								  fh_conf_peek_token (parser, 0)->column,
								  "Unexpected end of file");
		return NULL;
	}

	struct conf_token *token = fh_conf_peek_token (parser, 0);

	if (token->type != expected_type)
	{
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR,
								  token->line, token->column,
								  "Unexpected token '%s'", token->value);
		return NULL;
	}

	return fh_conf_consume_token (parser);
}

static struct conf_node *
fh_conf_parse_statement (struct fh_conf_parser *parser);
static struct conf_node *
fh_conf_parse_expr (struct fh_conf_parser *parser);

static struct conf_node *
fh_conf_parse_literal (struct fh_conf_parser *parser)
{
	struct conf_token *token = fh_conf_consume_token (parser);

	if (token->type != CONF_TOKEN_STRING && token->type != CONF_TOKEN_INT
		&& token->type != CONF_TOKEN_FLOAT && token->type != CONF_TOKEN_BOOLEAN)
	{
		fh_conf_parser_error (
			parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
			"Expected a literal value, got '%s'", token->value);
		return NULL;
	}

	struct conf_node *node
		= fh_conf_new_node (CONF_NODE_LITERAL, token->line, token->column);

	if (!node)
	{
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line,
								  token->column,
								  "Failed to allocate memory for literal node");
		return NULL;
	}

	node->details.literal.kind
		= token->type == CONF_TOKEN_STRING	? CONF_LITERAL_STRING
		  : token->type == CONF_TOKEN_INT	? CONF_LITERAL_INT
		  : token->type == CONF_TOKEN_FLOAT ? CONF_LITERAL_FLOAT
											: CONF_LITERAL_BOOLEAN;

	switch (node->details.literal.kind)
	{
		case CONF_LITERAL_STRING:
			node->details.literal.value.str.value
				= strndup (token->value, token->length);
			node->details.literal.value.str.length = token->length;

			if (!node->details.literal.value.str.value)
			{
				fh_conf_free_node (node);
				fh_conf_parser_error (
					parser, CONF_PARSER_ERROR_MEMORY, token->line,
					token->column,
					"Failed to allocate memory for string literal");
				return NULL;
			}
			break;

		case CONF_LITERAL_INT:
			{
				char *endptr;
				node->details.literal.value.int_value
					= strtoll (token->value, &endptr, 10);

				if (*endptr != 0)
				{
					fh_conf_free_node (node);
					fh_conf_parser_error (
						parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line,
						token->column, "Invalid integer literal '%s'",
						token->value);
					return NULL;
				}
			}

			break;

		case CONF_LITERAL_FLOAT:
			{
				char *endptr;
				node->details.literal.value.float_value
					= strtod (token->value, &endptr);

				if (*endptr != 0)
				{
					fh_conf_free_node (node);
					fh_conf_parser_error (
						parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line,
						token->column, "Invalid float literal '%s'",
						token->value);
					return NULL;
				}
			}

			break;

		case CONF_LITERAL_BOOLEAN:
			if (strcmp (token->value, "true") == 0
				|| strcmp (token->value, "yes") == 0)
			{
				node->details.literal.value.bool_value = true;
			}
			else if (strcmp (token->value, "false") == 0
					 || strcmp (token->value, "no") == 0)
			{
				node->details.literal.value.bool_value = false;
			}
			else
			{
				fh_conf_free_node (node);
				fh_conf_parser_error (
					parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line,
					token->column, "Invalid boolean literal '%s'",
					token->value);
				return NULL;
			}

			break;

		default:
			fh_conf_free_node (node);
			fh_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR,
									  token->line, token->column,
									  "Unexpected token '%s'", token->value);
			return NULL;
	}

	return node;
}

static struct conf_node *
fh_conf_parse_array (struct fh_conf_parser *parser)
{
	struct conf_token *token
		= fh_conf_expect_token (parser, CONF_TOKEN_OPEN_BRACKET);

	if (!token)
		return NULL;

	struct conf_node *node
		= fh_conf_new_node (CONF_NODE_ARRAY, token->line, token->column);

	if (!node)
	{
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line,
								  token->column,
								  "Failed to allocate memory for array node");
		return NULL;
	}

	node->details.array.elements = NULL;
	node->details.array.element_count = 0;

	while (!fh_conf_is_eof (parser)
		   && fh_conf_peek_token (parser, 0)->type
				  != CONF_TOKEN_CLOSE_BRACKET)
	{
		struct conf_node *element = fh_conf_parse_expr (parser);

		if (!element)
		{
			fh_conf_free_node (node);
			return NULL;
		}

		struct conf_node **new_elements
			= realloc (node->details.array.elements,
					   sizeof (struct conf_node *)
						   * (node->details.array.element_count + 1));

		if (!new_elements)
		{
			fh_conf_free_node (element);
			fh_conf_free_node (node);
			fh_conf_parser_error (
				parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
				"Failed to allocate memory for array elements");
			return NULL;
		}

		node->details.array.elements = new_elements;
		node->details.array.elements[node->details.array.element_count++]
			= element;
		element->parent = node;

		if (!fh_conf_is_eof (parser)
			&& fh_conf_peek_token (parser, 0)->type
				   == CONF_TOKEN_CLOSE_BRACKET)
			break;

		fh_conf_expect_token (parser, CONF_TOKEN_COMMA);
	}

	if (!fh_conf_expect_token (parser, CONF_TOKEN_CLOSE_BRACKET))
	{
		fh_conf_free_node (node);
		return NULL;
	}

	return node;
}

static struct conf_node *
fh_conf_parse_expr (struct fh_conf_parser *parser)
{
	struct conf_token *token = fh_conf_peek_token (parser, 0);

	switch (token->type)
	{
		case CONF_TOKEN_STRING:
		case CONF_TOKEN_INT:
		case CONF_TOKEN_FLOAT:
		case CONF_TOKEN_BOOLEAN:
			return fh_conf_parse_literal (parser);

		case CONF_TOKEN_OPEN_BRACKET:
			return fh_conf_parse_array (parser);

		default:
			fh_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR,
									  token->line, token->column,
									  "Expected an expression, got '%s'",
									  token->value);
			return NULL;
	}
}

static struct conf_node *
fh_conf_parse_identifier (struct fh_conf_parser *parser)
{
	struct conf_token *token
		= fh_conf_expect_token (parser, CONF_TOKEN_IDENTIFIER);

	if (!token)
		return NULL;

	struct conf_node *node = fh_conf_new_node (CONF_NODE_IDENTIFIER,
												   token->line, token->column);

	if (!node)
	{
		fh_conf_parser_error (
			parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
			"Failed to allocate memory for identifier node");
		return NULL;
	}

	node->details.identifier.value = strndup (token->value, token->length);

	if (!node->details.identifier.value)
	{
		fh_conf_free_node (node);
		fh_conf_parser_error (
			parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
			"Failed to allocate memory for identifier string");
		return NULL;
	}

	node->details.identifier.identifier_length = token->length;
	node->parent = NULL;

	return node;
}

static struct conf_node *
fh_conf_parse_assignment (struct fh_conf_parser *parser)
{
	struct conf_node *identifier = fh_conf_parse_identifier (parser);

	if (!identifier)
		return NULL;

	if (!fh_conf_expect_token (parser, CONF_TOKEN_EQUALS))
		return NULL;

	struct conf_node *right = fh_conf_parse_expr (parser);

	if (!right)
		return NULL;

	struct conf_node *node = fh_conf_new_node (
		CONF_NODE_ASSIGNMENT, identifier->line, identifier->column);

	if (!node)
	{
		fh_conf_free_node (right);
		fh_conf_parser_error (
			parser, CONF_PARSER_ERROR_MEMORY, identifier->line,
			identifier->column,
			"Failed to allocate memory for assignment node");
		return NULL;
	}

	node->details.assignment.left = identifier;
	node->details.assignment.right = right;

	return node;
}

static struct conf_node *
fh_conf_parse_block (struct fh_conf_parser *parser)
{
	struct conf_node *identifier = fh_conf_parse_identifier (parser);

	if (!identifier)
		return NULL;

	struct conf_node *block_node = fh_conf_new_node (
		CONF_NODE_BLOCK, identifier->line, identifier->column);

	if (!block_node)
	{
		fh_conf_free_node (identifier);
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY,
								  identifier->line, identifier->column,
								  "Failed to allocate memory for block node");
		return NULL;
	}

	block_node->details.block.name = identifier;
	block_node->details.block.children = NULL;
	block_node->details.block.child_count = 0;

	if (fh_conf_peek_token (parser, 0)->type == CONF_TOKEN_OPEN_PARENTHESIS)
	{
		fh_conf_consume_token (parser);

		block_node->details.block.args = NULL;
		block_node->details.block.argc = 0;

		while (!fh_conf_is_eof (parser)
			   && fh_conf_peek_token (parser, 0)->type
					  != CONF_TOKEN_CLOSE_PARENTHESIS)
		{
			struct conf_node *arg = fh_conf_parse_expr (parser);

			if (!arg)
			{
				fh_conf_free_node (block_node);
				return NULL;
			}

			struct conf_node **new_args
				= realloc (block_node->details.block.args,
						   sizeof (struct conf_node *)
							   * (block_node->details.block.argc + 1));

			if (!new_args)
			{
				fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY,
										  arg->line, arg->column,
										  "Failed to allocate memory for args");
				fh_conf_free_node (block_node);
				fh_conf_free_node (arg);
				return NULL;
			}

			block_node->details.block.args = new_args;
			block_node->details.block.args[block_node->details.block.argc++]
				= arg;
			arg->parent = NULL;

			if (!fh_conf_is_eof (parser)
				&& fh_conf_peek_token (parser, 0)->type
					   == CONF_TOKEN_CLOSE_PARENTHESIS)
				break;

			fh_conf_expect_token (parser, CONF_TOKEN_COMMA);
		}

		if (!fh_conf_expect_token (parser, CONF_TOKEN_CLOSE_PARENTHESIS))
		{
			fh_conf_free_node (block_node);
			return NULL;
		}
	}

	if (!fh_conf_expect_token (parser, CONF_TOKEN_OPEN_BRACE))
		return NULL;

	while (!fh_conf_is_eof (parser)
		   && fh_conf_peek_token (parser, 0)->type
				  != CONF_TOKEN_CLOSE_BRACE)
	{
		struct conf_node *child = fh_conf_parse_statement (parser);

		if (!child)
		{
			fh_conf_free_node (block_node);
			return NULL;
		}

		struct conf_node **new_children
			= realloc (block_node->details.block.children,
					   sizeof (struct conf_node *)
						   * (block_node->details.block.child_count + 1));

		if (!new_children)
		{
			fh_conf_free_node (child);
			fh_conf_free_node (block_node);
			fh_conf_parser_error (
				parser, CONF_PARSER_ERROR_MEMORY, identifier->line,
				identifier->column,
				"Failed to allocate memory for block children");
			return NULL;
		}

		block_node->details.block.children = new_children;
		block_node->details.block
			.children[block_node->details.block.child_count++]
			= child;
		child->parent = block_node;
	}

	if (!fh_conf_expect_token (parser, CONF_TOKEN_CLOSE_BRACE))
	{
		fh_conf_free_node (block_node);
		return NULL;
	}

	return block_node;
}

static struct conf_node *
fh_conf_parse_include (struct fh_conf_parser *parser)
{
	struct conf_token *token = fh_conf_consume_token (parser);

	if (!token
		|| (token->type != CONF_TOKEN_INCLUDE
			&& token->type != CONF_TOKEN_INCLUDE_OPTIONAL))
	{
		fh_conf_parser_error (
			parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
			"Expected 'include' or 'include_optional', got '%s'", token->value);
		return NULL;
	}

	struct conf_node *node
		= fh_conf_new_node (CONF_NODE_INCLUDE, token->line, token->column);

	if (!node)
	{
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line,
								  token->column,
								  "Failed to allocate memory for include node");
		return NULL;
	}

	node->details.include.optional = token->type == CONF_TOKEN_INCLUDE_OPTIONAL;

	if (!(token = fh_conf_expect_token (parser, CONF_TOKEN_STRING)))
	{
		fh_conf_free_node (node);
		return NULL;
	}

	node->details.include.filename = strndup (token->value, token->length);

	if (!node->details.include.filename)
	{
		fh_conf_free_node (node);
		fh_conf_parser_error (
			parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
			"Failed to allocate memory for include filename");
		return NULL;
	}

	return node;
}

static struct conf_node *
fh_conf_parse_statement (struct fh_conf_parser *parser)
{
	struct conf_node *node;

	if (fh_conf_peek_token (parser, 0)->type == CONF_TOKEN_INCLUDE
		|| fh_conf_peek_token (parser, 0)->type
			   == CONF_TOKEN_INCLUDE_OPTIONAL)
	{
		node = fh_conf_parse_include (parser);
	}
	else if (parser->token_index + 1 < parser->token_count
			 && parser->tokens[parser->token_index + 1].type
					== CONF_TOKEN_EQUALS)
	{
		node = fh_conf_parse_assignment (parser);
	}

	else if (parser->token_index + 1 < parser->token_count
			 && fh_conf_peek_token (parser, 0)->type
					== CONF_TOKEN_IDENTIFIER
			 && (fh_conf_peek_token (parser, 1)->type
					 == CONF_TOKEN_OPEN_PARENTHESIS
				 || fh_conf_peek_token (parser, 1)->type
						== CONF_TOKEN_OPEN_BRACE))
	{
		node = fh_conf_parse_block (parser);
	}
	else
	{
		node = fh_conf_parse_expr (parser);
	}

	while (!fh_conf_is_eof (parser)
		   && fh_conf_peek_token (parser, 0)->type == CONF_TOKEN_SEMICOLON)
		fh_conf_consume_token (parser);

	return node;
}

static struct conf_node *
fh_conf_parse (struct fh_conf_parser *parser)
{
	enum conf_parser_error rc;

	parser->last_error_code = CONF_PARSER_ERROR_NONE;

	if (!parser->tokens
		&& (rc = fh_conf_parser_tokenize (parser))
			   != CONF_PARSER_ERROR_NONE)
	{
		parser->last_error_code = rc;
		return NULL;
	}

	struct conf_node *root = fh_conf_new_node (CONF_NODE_ROOT, 1, 1);

	if (!root)
	{
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, 1, 1,
								  "Failed to allocate memory for root node");
		return NULL;
	}

	while (!fh_conf_is_eof (parser))
	{
		struct conf_node *node = fh_conf_parse_statement (parser);

		if (!node)
		{
			fh_conf_free_node (root);
			return NULL;
		}

		struct conf_node **new_children = realloc (
			root->details.root.children,
			sizeof (struct conf_node *) * (root->details.root.child_count + 1));

		if (!new_children)
		{
			fh_conf_free_node (node);
			fh_conf_free_node (root);
			fh_conf_parser_error (
				parser, CONF_PARSER_ERROR_MEMORY, 1, 1,
				"Failed to allocate memory for root children");
			return NULL;
		}

		root->details.root.children = new_children;
		root->details.root.children[root->details.root.child_count] = node;
		root->details.root.child_count++;
	}

	if (parser->last_error_code != CONF_PARSER_ERROR_NONE)
	{
		fh_conf_free_node (root);
		return NULL;
	}

	return root;
}

void
fh_conf_print_node (const struct conf_node *node, int indent)
{
	printf ("%*s<%p> ", indent, "", (void *) node);

	switch (node->type)
	{
		case CONF_NODE_ROOT:
			printf ("[Root Node]:\n");
			for (size_t i = 0; i < node->details.root.child_count; i++)
			{
				fh_conf_print_node (node->details.root.children[i],
										indent + 2);
			}
			break;

		case CONF_NODE_ASSIGNMENT:
			printf ("[Assignment]: %s = ",
					node->details.assignment.left->details.identifier.value);
			fh_conf_print_node (node->details.assignment.right, 0);
			break;

		case CONF_NODE_BLOCK:
			printf ("[Block]: %s [Parent <%p>]\n",
					node->details.block.name->details.identifier.value,
					(void *) node->parent);

			for (size_t i = 0; i < node->details.block.child_count; i++)
			{
				fh_conf_print_node (node->details.block.children[i],
										indent + 2);
			}
			break;

		case CONF_NODE_ARRAY:
			printf ("[Array]:\n");

			for (size_t i = 0; i < node->details.array.element_count; i++)
			{
				fh_conf_print_node (node->details.array.elements[i],
										indent + 2);
			}
			break;

		case CONF_NODE_IDENTIFIER:
			printf ("[Identifier]: %.*s\n",
					(int) node->details.identifier.identifier_length,
					node->details.identifier.value);
			break;

		case CONF_NODE_LITERAL:
			switch (node->details.literal.kind)
			{
				case CONF_LITERAL_INT:
					printf ("[Literal Int]: %ld\n",
							node->details.literal.value.int_value);
					break;
				case CONF_LITERAL_FLOAT:
					printf ("[Literal Float]: %f\n",
							node->details.literal.value.float_value);
					break;
				case CONF_LITERAL_STRING:
					printf ("[Literal String]: \"%.*s\"\n",
							(int) node->details.literal.value.str.length,
							node->details.literal.value.str.value);
					break;
				case CONF_LITERAL_BOOLEAN:
					printf ("[Literal Boolean]: %s\n",
							node->details.literal.value.bool_value ? "true"
																   : "false");
					break;
				default:
					printf ("[Literal: Unknown kind]\n");
					break;
			}

			break;
		case CONF_NODE_FUNCTION_CALL:
			printf ("[Function Call]: %.*s(",
					(int) node->details.function_call.function_name_length,
					node->details.function_call.function_name);

			for (size_t i = 0; i < node->details.function_call.argc; i++)
			{
				if (i > 0)
					printf (", ");
				fh_conf_print_node (node->details.function_call.args[i], 0);
			}

			printf (")\n");
			break;
		default:
			printf ("[Unknown Node Type]: %d\n", node->type);
			break;
	}
}

struct fh_config *
fh_conf_process (struct fh_conf_parser *parser, struct fh_traverse_ctx *ctx, struct fh_config *config)
{
	struct conf_node *root = fh_conf_parse (parser);

	if (!root)
		return NULL;

	struct fh_traverse_ctx stack_ctx = { 0 };
	bool ctx_allocated = false;

	if (!ctx)
	{
		ctx_allocated = true;

		if (!fh_conf_traverse_ctx_init (&stack_ctx, parser))
		{
			fh_pr_err ("Failed to initialize ctx");
			fh_conf_free_node (root);
			fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, 1, 1,
								  "Failed to initialize ctx");
			return NULL;
		}

		ctx = &stack_ctx;
	}

	bool allocated = false;

	if (!config)
	{
		allocated = true;
		config = calloc (1, sizeof (*config));

		if (!config)
		{
			if (ctx_allocated)
				fh_conf_traverse_ctx_free (ctx);

			fh_conf_free_node (root);
			fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, 1, 1,
									"Failed to allocate memory for config");
			return NULL;
		}

		if (!fh_conf_init (config))
		{
			if (ctx_allocated)
				fh_conf_traverse_ctx_free (ctx);

			free (config);
			fh_conf_free_node (root);
			fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, 1, 1,
									"Memory allocation error");
			return NULL;
		}
	}

	if (!fh_conf_traverse (ctx, root, config))
	{
		if (ctx_allocated)
			fh_conf_traverse_ctx_free (ctx);

		fh_conf_free_node (root);

		if (allocated)
			fh_conf_free (config);

		return NULL;
	}

	if (ctx_allocated)
		fh_conf_traverse_ctx_free (ctx);

	fh_conf_free_node (root);

	if (!config->default_host_config && parser->root_parser)
	{
		if (allocated)
			fh_conf_free (config);

		fh_conf_parser_error (
			parser, CONF_PARSER_ERROR_INVALID_CONFIG, 1, 1,
			"At least one default host (...) {...} definition is required");
		return NULL;
	}

	return config;
}
