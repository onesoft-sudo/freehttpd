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

#include "conf.h"
#include "strutils.h"

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
					char *str_value;
					size_t str_length;
				};
			};
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
	};
};

struct fhttpd_conf_parser
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

static struct fhttpd_conf_parser *
fhttpd_conf_parser_create_internal (const char *filename, bool is_root)
{
	struct fhttpd_conf_parser *parser = calloc (1, sizeof (*parser));

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

struct fhttpd_conf_parser *
fhttpd_conf_parser_create (const char *filename)
{
	return fhttpd_conf_parser_create_internal (filename, true);
}

int
fhttpd_conf_parser_read (struct fhttpd_conf_parser *parser)
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
			= fread (parser->source + parser->source_len, 1, (size_t) file_size - parser->source_len, file);

		if (bytes_read == 0)
			break;

		parser->source_len += bytes_read;
	}

	if (ferror (file) || !feof (file) || parser->source_len != (size_t) file_size)
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
fhttpd_conf_parser_free_token (struct conf_token *token)
{
	free (token->value);
}

static void
fhttpd_conf_parser_error (struct fhttpd_conf_parser *parser, enum conf_parser_error code, size_t line, size_t column,
						  const char *message, ...)
{
	if (parser->last_error)
		free (parser->last_error);

	parser->last_error = NULL;
	parser->last_error_code = code;
	parser->error_line = line;
	parser->error_column = column;

	va_list args;
	va_start (args, message);
	vasprintf (&parser->last_error, message, args);
	va_end (args);
}

static enum conf_parser_error
fhttpd_conf_parser_add_token (struct fhttpd_conf_parser *parser, size_t line, size_t column, enum conf_token_type type,
							  const char *value, size_t length)
{
	struct conf_token *tokens = realloc (parser->tokens, sizeof (struct conf_token) * (parser->token_count + 1));

	if (!tokens)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, line, column,
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
fhttpd_conf_parser_destroy (struct fhttpd_conf_parser *parser)
{
	if (!parser)
		return;

	if (parser->tokens)
	{
		for (size_t i = 0; i < parser->token_count; i++)
		{
			fhttpd_conf_parser_free_token (&parser->tokens[i]);
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
fhttpd_conf_parser_tokenize (struct fhttpd_conf_parser *parser)
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
			if ((rc = fhttpd_conf_parser_add_token (parser, line, column, token_type, &c, 1)) != CONF_PARSER_ERROR_NONE)
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

			while (i < parser->source_len && parser->source[i] != '"' && parser->source[i] != '\"')
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
				fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, line, column,
										  "Unterminated string literal");
				return CONF_PARSER_ERROR_SYNTAX_ERROR;
			}

			size_t length = i - start;

			if ((rc = fhttpd_conf_parser_add_token (parser, start_line, start_column, CONF_TOKEN_STRING,
													&parser->source[start], length))
				!= CONF_PARSER_ERROR_NONE)
				return rc;

			column++;
			i++;
			continue;
		}

		if (isdigit (c) || (c == '-' && i + 1 < parser->source_len && isdigit (parser->source[i + 1])))
		{
			size_t start = i;
			bool is_float = false;
			size_t start_line = line, start_column = column;

			while (i < parser->source_len
				   && (isdigit (parser->source[i]) || parser->source[i] == '.' || parser->source[i] == '-'))
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
							fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, line, column,
													  "Multiple decimal points in number");
							return CONF_PARSER_ERROR_SYNTAX_ERROR;
						}

						is_float = true;
						break;

					case '-':
						if (i != start)
						{
							fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, line, column,
													  "Unexpected '-' in number");
							return CONF_PARSER_ERROR_SYNTAX_ERROR;
						}

						break;

					default:
						break;
				}

				i++;
			}

			size_t length = i - start;

			if ((rc = fhttpd_conf_parser_add_token (parser, start_line, start_column,
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

			while (i < parser->source_len && (isalnum (parser->source[i]) || parser->source[i] == '_'))
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
			else if (strncmp (&parser->source[start], "include_optional", length) == 0)
				token_type = CONF_TOKEN_INCLUDE_OPTIONAL;

			if ((rc = fhttpd_conf_parser_add_token (parser, start_line, start_column, token_type,
													&parser->source[start], length))
				!= CONF_PARSER_ERROR_NONE)
				return rc;

			continue;
		}

		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, line, column, "Unexpected character '%c'", c);
		return CONF_PARSER_ERROR_SYNTAX_ERROR;
	}

	if ((rc = fhttpd_conf_parser_add_token (parser, line, column, CONF_TOKEN_EOF, "[EOF]", 5))
		!= CONF_PARSER_ERROR_NONE)
		return rc;

	return CONF_PARSER_ERROR_NONE;
}

static void
fhttpd_conf_parser_print_tokens (struct fhttpd_conf_parser *parser)
{
	for (size_t i = 0; i < parser->token_count; i++)
	{
		struct conf_token *token = &parser->tokens[i];

		printf ("[%zu]: Token <type=%d, value='%.*s', line=%zu, column=%zu>\n", i, token->type, (int) token->length,
				token->value, token->line, token->column);
	}
}

enum conf_parser_error
fhttpd_conf_parser_last_error (struct fhttpd_conf_parser *parser)
{
	return parser->last_error_code;
}

bool
fhttpd_conf_parser_print_error (struct fhttpd_conf_parser *parser)
{
	if (parser->last_error)
		fprintf (stderr, "%s:%zu:%zu: %s\n", parser->error_filename ? parser->error_filename : parser->filename,
				 parser->error_line, parser->error_column, parser->last_error);

	return parser->last_error != NULL;
}

const char *
fhttpd_conf_parser_strerror (enum conf_parser_error error)
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

static const char *
fhttpd_conf_strtoken (enum conf_token_type type)
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
fhttpd_conf_new_node (enum conf_node_type type, size_t line, size_t column)
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
fhttpd_conf_free_node (struct conf_node *node)
{
	if (!node)
		return;

	switch (node->type)
	{
		case CONF_NODE_ROOT:
			for (size_t i = 0; i < node->root.child_count; i++)
			{
				fhttpd_conf_free_node (node->root.children[i]);
			}

			free (node->root.children);
			break;

		case CONF_NODE_ASSIGNMENT:
			fhttpd_conf_free_node (node->assignment.left);
			fhttpd_conf_free_node (node->assignment.right);
			break;

		case CONF_NODE_BLOCK:
			for (size_t i = 0; i < node->block.child_count; i++)
			{
				fhttpd_conf_free_node (node->block.children[i]);
			}

			for (size_t i = 0; i < node->block.argc; i++)
			{
				fhttpd_conf_free_node (node->block.args[i]);
			}

			fhttpd_conf_free_node (node->block.name);
			free (node->block.args);
			free (node->block.children);
			break;

		case CONF_NODE_ARRAY:
			for (size_t i = 0; i < node->array.element_count; i++)
			{
				fhttpd_conf_free_node (node->array.elements[i]);
			}

			free (node->array.elements);
			break;

		case CONF_NODE_IDENTIFIER:
			free (node->identifier.value);
			break;

		case CONF_NODE_LITERAL:
			if (node->literal.kind == CONF_LITERAL_STRING)
				free (node->literal.str_value);

			break;

		case CONF_NODE_FUNCTION_CALL:
			free (node->function_call.function_name);

			for (size_t i = 0; i < node->function_call.argc; i++)
			{
				fhttpd_conf_free_node (node->function_call.args[i]);
			}

			free (node->function_call.args);
			break;

		case CONF_NODE_INCLUDE:
			free (node->include.filename);
			break;

		default:
			break;
	}

	free (node);
}

static inline bool
fhttpd_conf_is_eof (struct fhttpd_conf_parser *parser)
{
	return parser->token_index >= parser->token_count || parser->tokens[parser->token_index].type == CONF_TOKEN_EOF;
}

static inline struct conf_token *
fhttpd_conf_peek_token (struct fhttpd_conf_parser *parser, size_t offset)
{
	if (parser->token_index + offset >= parser->token_count)
		return NULL;

	return &parser->tokens[parser->token_index + offset];
}

static inline struct conf_token *
fhttpd_conf_consume_token (struct fhttpd_conf_parser *parser)
{
	if (parser->token_index >= parser->token_count)
		return NULL;

	return &parser->tokens[parser->token_index++];
}

static struct conf_token *
fhttpd_conf_expect_token (struct fhttpd_conf_parser *parser, enum conf_token_type expected_type)
{
	if (fhttpd_conf_is_eof (parser))
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, fhttpd_conf_peek_token (parser, 0)->line,
								  fhttpd_conf_peek_token (parser, 0)->column, "Unexpected end of file");
		return NULL;
	}

	struct conf_token *token = fhttpd_conf_peek_token (parser, 0);

	if (token->type != expected_type)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
								  "Unexpected token '%s'", token->value);
		return NULL;
	}

	return fhttpd_conf_consume_token (parser);
}

static struct conf_node *fhttpd_conf_parse_statement (struct fhttpd_conf_parser *parser);
static struct conf_node *fhttpd_conf_parse_expr (struct fhttpd_conf_parser *parser);

static struct conf_node *
fhttpd_conf_parse_literal (struct fhttpd_conf_parser *parser)
{
	struct conf_token *token = fhttpd_conf_consume_token (parser);

	if (token->type != CONF_TOKEN_STRING && token->type != CONF_TOKEN_INT && token->type != CONF_TOKEN_FLOAT
		&& token->type != CONF_TOKEN_BOOLEAN)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
								  "Expected a literal value, got '%s'", token->value);
		return NULL;
	}

	struct conf_node *node = fhttpd_conf_new_node (CONF_NODE_LITERAL, token->line, token->column);

	if (!node)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
								  "Failed to allocate memory for literal node");
		return NULL;
	}

	node->literal.kind = token->type == CONF_TOKEN_STRING  ? CONF_LITERAL_STRING
						 : token->type == CONF_TOKEN_INT   ? CONF_LITERAL_INT
						 : token->type == CONF_TOKEN_FLOAT ? CONF_LITERAL_FLOAT
														   : CONF_LITERAL_BOOLEAN;

	switch (node->literal.kind)
	{
		case CONF_LITERAL_STRING:
			node->literal.str_value = strndup (token->value, token->length);
			node->literal.str_length = token->length;

			if (!node->literal.str_value)
			{
				fhttpd_conf_free_node (node);
				fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
										  "Failed to allocate memory for string literal");
				return NULL;
			}
			break;

		case CONF_LITERAL_INT:
			{
				char *endptr;
				node->literal.int_value = strtoll (token->value, &endptr, 10);

				if (*endptr != 0)
				{
					fhttpd_conf_free_node (node);
					fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
											  "Invalid integer literal '%s'", token->value);
					return NULL;
				}
			}

			break;

		case CONF_LITERAL_FLOAT:
			{
				char *endptr;
				node->literal.float_value = strtod (token->value, &endptr);

				if (*endptr != 0)
				{
					fhttpd_conf_free_node (node);
					fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
											  "Invalid float literal '%s'", token->value);
					return NULL;
				}
			}

			break;

		case CONF_LITERAL_BOOLEAN:
			if (strcmp (token->value, "true") == 0 || strcmp (token->value, "yes") == 0)
			{
				node->literal.bool_value = true;
			}
			else if (strcmp (token->value, "false") == 0 || strcmp (token->value, "no") == 0)
			{
				node->literal.bool_value = false;
			}
			else
			{
				fhttpd_conf_free_node (node);
				fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
										  "Invalid boolean literal '%s'", token->value);
				return NULL;
			}

			break;

		default:
			fhttpd_conf_free_node (node);
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
									  "Unexpected token '%s'", token->value);
			return NULL;
	}

	return node;
}

static struct conf_node *
fhttpd_conf_parse_array (struct fhttpd_conf_parser *parser)
{
	struct conf_token *token = fhttpd_conf_expect_token (parser, CONF_TOKEN_OPEN_BRACKET);

	if (!token)
		return NULL;

	struct conf_node *node = fhttpd_conf_new_node (CONF_NODE_ARRAY, token->line, token->column);

	if (!node)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
								  "Failed to allocate memory for array node");
		return NULL;
	}

	node->array.elements = NULL;
	node->array.element_count = 0;

	while (!fhttpd_conf_is_eof (parser) && fhttpd_conf_peek_token (parser, 0)->type != CONF_TOKEN_CLOSE_BRACKET)
	{
		struct conf_node *element = fhttpd_conf_parse_expr (parser);

		if (!element)
		{
			fhttpd_conf_free_node (node);
			return NULL;
		}

		struct conf_node **new_elements
			= realloc (node->array.elements, sizeof (struct conf_node *) * (node->array.element_count + 1));

		if (!new_elements)
		{
			fhttpd_conf_free_node (element);
			fhttpd_conf_free_node (node);
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
									  "Failed to allocate memory for array elements");
			return NULL;
		}

		node->array.elements = new_elements;
		node->array.elements[node->array.element_count++] = element;
		element->parent = node;

		if (!fhttpd_conf_is_eof (parser) && fhttpd_conf_peek_token (parser, 0)->type == CONF_TOKEN_CLOSE_BRACKET)
			break;

		fhttpd_conf_expect_token (parser, CONF_TOKEN_COMMA);
	}

	if (!fhttpd_conf_expect_token (parser, CONF_TOKEN_CLOSE_BRACKET))
	{
		fhttpd_conf_free_node (node);
		return NULL;
	}

	return node;
}

static struct conf_node *
fhttpd_conf_parse_expr (struct fhttpd_conf_parser *parser)
{
	struct conf_token *token = fhttpd_conf_peek_token (parser, 0);

	switch (token->type)
	{
		case CONF_TOKEN_STRING:
		case CONF_TOKEN_INT:
		case CONF_TOKEN_FLOAT:
		case CONF_TOKEN_BOOLEAN:
			return fhttpd_conf_parse_literal (parser);

		case CONF_TOKEN_OPEN_BRACKET:
			return fhttpd_conf_parse_array (parser);

		default:
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
									  "Expected an expression, got '%s'", token->value);
			return NULL;
	}
}

static struct conf_node *
fhttpd_conf_parse_identifier (struct fhttpd_conf_parser *parser)
{
	struct conf_token *token = fhttpd_conf_expect_token (parser, CONF_TOKEN_IDENTIFIER);

	if (!token)
		return NULL;

	struct conf_node *node = fhttpd_conf_new_node (CONF_NODE_IDENTIFIER, token->line, token->column);

	if (!node)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
								  "Failed to allocate memory for identifier node");
		return NULL;
	}

	node->identifier.value = strndup (token->value, token->length);

	if (!node->identifier.value)
	{
		fhttpd_conf_free_node (node);
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
								  "Failed to allocate memory for identifier string");
		return NULL;
	}

	node->identifier.identifier_length = token->length;
	node->parent = NULL;

	return node;
}

static struct conf_node *
fhttpd_conf_parse_assignment (struct fhttpd_conf_parser *parser)
{
	struct conf_node *identifier = fhttpd_conf_parse_identifier (parser);

	if (!identifier)
		return NULL;

	if (!fhttpd_conf_expect_token (parser, CONF_TOKEN_EQUALS))
		return NULL;

	struct conf_node *right = fhttpd_conf_parse_expr (parser);

	if (!right)
		return NULL;

	struct conf_node *node = fhttpd_conf_new_node (CONF_NODE_ASSIGNMENT, identifier->line, identifier->column);

	if (!node)
	{
		fhttpd_conf_free_node (right);
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, identifier->line, identifier->column,
								  "Failed to allocate memory for assignment node");
		return NULL;
	}

	node->assignment.left = identifier;
	node->assignment.right = right;

	return node;
}

static struct conf_node *
fhttpd_conf_parse_block (struct fhttpd_conf_parser *parser)
{
	struct conf_node *identifier = fhttpd_conf_parse_identifier (parser);

	if (!identifier)
		return NULL;

	struct conf_node *block_node = fhttpd_conf_new_node (CONF_NODE_BLOCK, identifier->line, identifier->column);

	if (!block_node)
	{
		fhttpd_conf_free_node (identifier);
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, identifier->line, identifier->column,
								  "Failed to allocate memory for block node");
		return NULL;
	}

	block_node->block.name = identifier;
	block_node->block.children = NULL;
	block_node->block.child_count = 0;

	if (fhttpd_conf_peek_token (parser, 0)->type == CONF_TOKEN_OPEN_PARENTHESIS)
	{
		fhttpd_conf_consume_token (parser);

		block_node->block.args = NULL;
		block_node->block.argc = 0;

		while (!fhttpd_conf_is_eof (parser) && fhttpd_conf_peek_token (parser, 0)->type != CONF_TOKEN_CLOSE_PARENTHESIS)
		{
			struct conf_node *arg = fhttpd_conf_parse_expr (parser);

			if (!arg)
			{
				fhttpd_conf_free_node (block_node);
				return NULL;
			}

			struct conf_node **new_args
				= realloc (block_node->block.args, sizeof (struct conf_node *) * (block_node->block.argc + 1));

			if (!new_args)
			{
				fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, arg->line, arg->column,
										  "Failed to allocate memory for args");
				fhttpd_conf_free_node (block_node);
				fhttpd_conf_free_node (arg);
				return NULL;
			}

			block_node->block.args = new_args;
			block_node->block.args[block_node->block.argc++] = arg;
			arg->parent = NULL;

			if (!fhttpd_conf_is_eof (parser)
				&& fhttpd_conf_peek_token (parser, 0)->type == CONF_TOKEN_CLOSE_PARENTHESIS)
				break;

			fhttpd_conf_expect_token (parser, CONF_TOKEN_COMMA);
		}

		if (!fhttpd_conf_expect_token (parser, CONF_TOKEN_CLOSE_PARENTHESIS))
		{
			fhttpd_conf_free_node (block_node);
			return NULL;
		}
	}

	if (!fhttpd_conf_expect_token (parser, CONF_TOKEN_OPEN_BRACE))
		return NULL;

	while (!fhttpd_conf_is_eof (parser) && fhttpd_conf_peek_token (parser, 0)->type != CONF_TOKEN_CLOSE_BRACE)
	{
		struct conf_node *child = fhttpd_conf_parse_statement (parser);

		if (!child)
		{
			fhttpd_conf_free_node (block_node);
			return NULL;
		}

		struct conf_node **new_children
			= realloc (block_node->block.children, sizeof (struct conf_node *) * (block_node->block.child_count + 1));

		if (!new_children)
		{
			fhttpd_conf_free_node (child);
			fhttpd_conf_free_node (block_node);
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, identifier->line, identifier->column,
									  "Failed to allocate memory for block children");
			return NULL;
		}

		block_node->block.children = new_children;
		block_node->block.children[block_node->block.child_count++] = child;
		child->parent = block_node;
	}

	if (!fhttpd_conf_expect_token (parser, CONF_TOKEN_CLOSE_BRACE))
	{
		fhttpd_conf_free_node (block_node);
		return NULL;
	}

	return block_node;
}

static struct conf_node *
fhttpd_conf_parse_include (struct fhttpd_conf_parser *parser)
{
	struct conf_token *token = fhttpd_conf_consume_token (parser);

	if (!token || (token->type != CONF_TOKEN_INCLUDE && token->type != CONF_TOKEN_INCLUDE_OPTIONAL))
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_SYNTAX_ERROR, token->line, token->column,
								  "Expected 'include' or 'include_optional', got '%s'", token->value);
		return NULL;
	}

	struct conf_node *node = fhttpd_conf_new_node (CONF_NODE_INCLUDE, token->line, token->column);

	if (!node)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
								  "Failed to allocate memory for include node");
		return NULL;
	}

	node->include.optional = token->type == CONF_TOKEN_INCLUDE_OPTIONAL;

	if (!(token = fhttpd_conf_expect_token (parser, CONF_TOKEN_STRING)))
	{
		fhttpd_conf_free_node (node);
		return NULL;
	}

	node->include.filename = strndup (token->value, token->length);

	if (!node->include.filename)
	{
		fhttpd_conf_free_node (node);
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, token->line, token->column,
								  "Failed to allocate memory for include filename");
		return NULL;
	}

	return node;
}

static struct conf_node *
fhttpd_conf_parse_statement (struct fhttpd_conf_parser *parser)
{
	struct conf_node *node;

	if (fhttpd_conf_peek_token (parser, 0)->type == CONF_TOKEN_INCLUDE
		|| fhttpd_conf_peek_token (parser, 0)->type == CONF_TOKEN_INCLUDE_OPTIONAL)
	{
		node = fhttpd_conf_parse_include (parser);
	}
	else if (parser->token_index + 1 < parser->token_count
			 && parser->tokens[parser->token_index + 1].type == CONF_TOKEN_EQUALS)
	{
		node = fhttpd_conf_parse_assignment (parser);
	}

	else if (parser->token_index + 1 < parser->token_count
			 && fhttpd_conf_peek_token (parser, 0)->type == CONF_TOKEN_IDENTIFIER
			 && (fhttpd_conf_peek_token (parser, 1)->type == CONF_TOKEN_OPEN_PARENTHESIS
				 || fhttpd_conf_peek_token (parser, 1)->type == CONF_TOKEN_OPEN_BRACE))
	{
		node = fhttpd_conf_parse_block (parser);
	}
	else
	{
		node = fhttpd_conf_parse_expr (parser);
	}

	while (!fhttpd_conf_is_eof (parser) && fhttpd_conf_peek_token (parser, 0)->type == CONF_TOKEN_SEMICOLON)
		fhttpd_conf_consume_token (parser);

	return node;
}

static struct conf_node *
fhttpd_conf_parse (struct fhttpd_conf_parser *parser)
{
	enum conf_parser_error rc;

	parser->last_error_code = CONF_PARSER_ERROR_NONE;

	if (!parser->tokens && (rc = fhttpd_conf_parser_tokenize (parser)) != CONF_PARSER_ERROR_NONE)
	{
		parser->last_error_code = rc;
		return NULL;
	}

	struct conf_node *root = fhttpd_conf_new_node (CONF_NODE_ROOT, 1, 1);

	if (!root)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, 1, 1, "Failed to allocate memory for root node");
		return NULL;
	}

	while (!fhttpd_conf_is_eof (parser))
	{
		struct conf_node *node = fhttpd_conf_parse_statement (parser);

		if (!node)
		{
			fhttpd_conf_free_node (root);
			return NULL;
		}

		struct conf_node **new_children
			= realloc (root->root.children, sizeof (struct conf_node *) * (root->root.child_count + 1));

		if (!new_children)
		{
			fhttpd_conf_free_node (node);
			fhttpd_conf_free_node (root);
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, 1, 1,
									  "Failed to allocate memory for root children");
			return NULL;
		}

		root->root.children = new_children;
		root->root.children[root->root.child_count] = node;
		root->root.child_count++;
	}

	if (parser->last_error_code != CONF_PARSER_ERROR_NONE)
	{
		fhttpd_conf_free_node (root);
		return NULL;
	}

	return root;
}

static void
fhttpd_conf_print_node (struct conf_node *node, int indent)
{
	printf ("%*s<%p> ", indent, "", (void *) node);

	switch (node->type)
	{
		case CONF_NODE_ROOT:
			printf ("[Root Node]:\n");
			for (size_t i = 0; i < node->root.child_count; i++)
			{
				fhttpd_conf_print_node (node->root.children[i], indent + 2);
			}
			break;

		case CONF_NODE_ASSIGNMENT:
			printf ("[Assignment]: %s = ", node->assignment.left->identifier.value);
			fhttpd_conf_print_node (node->assignment.right, 0);
			break;

		case CONF_NODE_BLOCK:
			printf ("[Block]: %s [Parent <%p>]\n", node->block.name->identifier.value, (void *) node->parent);

			for (size_t i = 0; i < node->block.child_count; i++)
			{
				fhttpd_conf_print_node (node->block.children[i], indent + 2);
			}
			break;

		case CONF_NODE_ARRAY:
			printf ("[Array]:\n");

			for (size_t i = 0; i < node->array.element_count; i++)
			{
				fhttpd_conf_print_node (node->array.elements[i], indent + 2);
			}
			break;

		case CONF_NODE_IDENTIFIER:
			printf ("[Identifier]: %.*s\n", (int) node->identifier.identifier_length, node->identifier.value);
			break;

		case CONF_NODE_LITERAL:
			switch (node->literal.kind)
			{
				case CONF_LITERAL_INT:
					printf ("[Literal Int]: %ld\n", node->literal.int_value);
					break;
				case CONF_LITERAL_FLOAT:
					printf ("[Literal Float]: %f\n", node->literal.float_value);
					break;
				case CONF_LITERAL_STRING:
					printf ("[Literal String]: \"%.*s\"\n", (int) node->literal.str_length, node->literal.str_value);
					break;
				case CONF_LITERAL_BOOLEAN:
					printf ("[Literal Boolean]: %s\n", node->literal.bool_value ? "true" : "false");
					break;
				default:
					printf ("[Literal: Unknown kind]\n");
					break;
			}

			break;
		case CONF_NODE_FUNCTION_CALL:
			printf ("[Function Call]: %.*s(", (int) node->function_call.function_name_length,
					node->function_call.function_name);

			for (size_t i = 0; i < node->function_call.argc; i++)
			{
				if (i > 0)
					printf (", ");
				fhttpd_conf_print_node (node->function_call.args[i], 0);
			}

			printf (")\n");
			break;
		default:
			printf ("[Unknown Node Type]: %d\n", node->type);
			break;
	}
}

enum conf_value_type
{
	CONF_VALUE_TYPE_STRING,
	CONF_VALUE_TYPE_INT,
	CONF_VALUE_TYPE_FLOAT,
	CONF_VALUE_TYPE_BOOLEAN,
	CONF_VALUE_TYPE_NULL,
	CONF_VALUE_TYPE_ARRAY,
};

struct fhttpd_config_property
{
	const char *name;
	enum conf_value_type type;
	bool (*validator) (struct fhttpd_conf_parser *parser,
					   const struct conf_node *value); /* Optional validator function */
	bool (*setter) (struct fhttpd_conf_parser *parser, struct fhttpd_config *config,
					const struct conf_node *value); /* Setter function */
};

#define PROP_STRING_MUST_NOT_BE_EMPTY(prop, value)                                                                     \
	if ((value)->literal.str_length == 0)                                                                              \
	{                                                                                                                  \
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line, value->column,                \
								  "'" prop "' property cannot be an empty string");                                    \
		return false;                                                                                                  \
	}

static bool
fhttpd_prop_set_root (struct fhttpd_conf_parser *parser, struct fhttpd_config *config, const struct conf_node *value)
{
	PROP_STRING_MUST_NOT_BE_EMPTY ("root", value);

	if (value->literal.str_value[0] != '/')
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line, value->column,
								  "'root' property must be a valid absolute path starting with '/'");
		return false;
	}

	if (config->conf_root)
		free (config->conf_root);

	config->conf_root = strndup (value->literal.str_value, value->literal.str_length);

	if (!config->conf_root)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, value->line, value->column,
								  "Failed to allocate memory for 'root' property");
		return false;
	}

	return true;
}

static bool
fhttpd_prop_set_logging_min_level (struct fhttpd_conf_parser *parser, struct fhttpd_config *config,
								   const struct conf_node *value)
{
	PROP_STRING_MUST_NOT_BE_EMPTY ("logging.min_level", value);

	int level = strcasecmp (value->literal.str_value, "debug") == 0	  ? FHTTPD_LOG_LEVEL_DEBUG
				: strcasecmp (value->literal.str_value, "info") == 0  ? FHTTPD_LOG_LEVEL_INFO
				: strcasecmp (value->literal.str_value, "warn") == 0  ? FHTTPD_LOG_LEVEL_WARNING
				: strcasecmp (value->literal.str_value, "error") == 0 ? FHTTPD_LOG_LEVEL_ERROR
																	  : -1;

	if (level < 0)
	{
		fhttpd_conf_parser_error (
			parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line, value->column,
			"Invalid value for logging.min_level: '%.*s', expected one of: debug, info, warn, error",
			(int) value->literal.str_length, value->literal.str_value);
		return false;
	}

	config->logging_min_level = level;
	return true;
}

static bool
fhttpd_prop_set_logging_file (struct fhttpd_conf_parser *parser, struct fhttpd_config *config,
							  const struct conf_node *value)
{
	PROP_STRING_MUST_NOT_BE_EMPTY ("logging.file", value);

	if (config->logging_file)
		free (config->logging_file);

	config->logging_file = strndup (value->literal.str_value, value->literal.str_length);

	if (!config->logging_file)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, value->line, value->column,
								  "Failed to allocate memory for 'logging.file' property");
		return false;
	}

	return true;
}

static bool
fhttpd_prop_set_logging_error_file (struct fhttpd_conf_parser *parser, struct fhttpd_config *config,
									const struct conf_node *value)
{
	PROP_STRING_MUST_NOT_BE_EMPTY ("logging.error_file", value);

	if (config->logging_error_file)
		free (config->logging_error_file);

	config->logging_error_file = strndup (value->literal.str_value, value->literal.str_length);

	if (!config->logging_error_file)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, value->line, value->column,
								  "Failed to allocate memory for 'logging.error_file' property");
		return false;
	}

	return true;
}

static bool
fhttpd_prop_set_host_docroot (struct fhttpd_conf_parser *parser, struct fhttpd_config *config,
									const struct conf_node *value)
{
	PROP_STRING_MUST_NOT_BE_EMPTY ("host.docroot", value);

	if (config->docroot)
		free (config->docroot);

	config->docroot = strndup (value->literal.str_value, value->literal.str_length);

	if (!config->docroot)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, value->line, value->column,
								  "Failed to allocate memory for 'host.docroot' property");
		return false;
	}

	return true;
}

static const char *valid_blocks[] = { "logging", "host", NULL };

/* Property descriptor, validator and handler definitions */
static struct fhttpd_config_property const properties[] = {
	{
		"root",
		CONF_VALUE_TYPE_STRING,
		NULL,
		&fhttpd_prop_set_root,
	},
	{
		"logging.min_level",
		CONF_VALUE_TYPE_STRING,
		NULL,
		&fhttpd_prop_set_logging_min_level,
	},
	{
		"logging.file",
		CONF_VALUE_TYPE_STRING,
		NULL,
		&fhttpd_prop_set_logging_file,
	},
	{
		"logging.error_file",
		CONF_VALUE_TYPE_STRING,
		NULL,
		&fhttpd_prop_set_logging_error_file,
	},
	{
		"host.docroot",
		CONF_VALUE_TYPE_STRING,
		NULL,
		&fhttpd_prop_set_host_docroot,
	},
	{
		NULL,
		0,
		NULL,
		NULL,
	},
};

static const char *
fhttpd_conf_strtype (enum conf_value_type type)
{
	switch (type)
	{
		case CONF_VALUE_TYPE_STRING:
			return "string";
		case CONF_VALUE_TYPE_INT:
			return "int";
		case CONF_VALUE_TYPE_FLOAT:
			return "float";
		case CONF_VALUE_TYPE_BOOLEAN:
			return "boolean";
		case CONF_VALUE_TYPE_NULL:
			return "null";
		case CONF_VALUE_TYPE_ARRAY:
			return "array";
		default:
			return "unknown";
	}
}

static enum conf_value_type
fhttpd_conf_typeof (const struct conf_node *value)
{
	switch (value->type)
	{
		case CONF_NODE_LITERAL:
			switch (value->literal.kind)
			{
				case CONF_LITERAL_STRING:
					return CONF_VALUE_TYPE_STRING;
				case CONF_LITERAL_INT:
					return CONF_VALUE_TYPE_INT;
				case CONF_LITERAL_FLOAT:
					return CONF_VALUE_TYPE_FLOAT;
				case CONF_LITERAL_BOOLEAN:
					return CONF_VALUE_TYPE_BOOLEAN;
				default:
					return CONF_VALUE_TYPE_NULL;
			}

		case CONF_NODE_ARRAY:
			return CONF_VALUE_TYPE_ARRAY;

		default:
			assert (false && "Unexpected node type in fhttpd_conf_typeof");
			return CONF_VALUE_TYPE_NULL;
	}
}

static bool
fhttpd_conf_expect_property (struct fhttpd_conf_parser *parser, const struct fhttpd_config_property *descriptor,
							 const char *block, enum conf_value_type type, const struct conf_node *value)
{
	if (fhttpd_conf_typeof (value) != type)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line, value->column,
								  "Expected property '%s' in block context '%s' to be of type '%s', got '%s'",
								  descriptor->name, block, fhttpd_conf_strtype (type),
								  fhttpd_conf_strtype (fhttpd_conf_typeof (value)));
		return false;
	}

	return true;
}

static bool
fhttpd_conf_node_walk_assignment (struct fhttpd_conf_parser *parser, const struct conf_node *node,
								  struct fhttpd_config *config)
{
	assert (node != NULL);
	assert (node->assignment.left != NULL && node->assignment.left->type == CONF_NODE_IDENTIFIER);
	assert (node->assignment.right != NULL);

	const char *identifier = node->assignment.left->identifier.value;
	const char *block = NULL;
	bool valid_block = false, is_root_block = false;

	if (!node->parent)
	{
		block = "root";
		valid_block = true;
		is_root_block = true;
	}
	else if (node->parent->type != CONF_NODE_BLOCK)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line, node->column,
								  "Assignment outside of a block context");
		return false;
	}
	else
	{
		block = node->parent->block.name->identifier.value;
	}

	if (!valid_block)
	{
		for (size_t i = 0; valid_blocks[i]; i++)
		{
			if (strcmp (valid_blocks[i], block) == 0)
			{
				valid_block = true;
				break;
			}
		}
	}

	if (!valid_block)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line, node->column,
								  "Invalid block context '%s'", block);
		return false;
	}

	bool valid_property = false;
	char property_name[256] = { 0 };

	if (strlen (identifier) >= sizeof (property_name))
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line, node->column,
								  "Unknown property '%s' in block context '%s'", identifier, block);
		return false;
	}

	if (!is_root_block)
		snprintf (property_name, sizeof (property_name), "%.64s.%.128s", block, identifier);
	else
		strncpy (property_name, identifier, sizeof (property_name) - 1);

	for (size_t i = 0; properties[i].name; i++)
	{
		if (!strcmp (properties[i].name, property_name))
		{
			if (!fhttpd_conf_expect_property (parser, &properties[i], block, properties[i].type,
											  node->assignment.right))
				return false;

			if (properties[i].validator && !properties[i].validator (parser, node->assignment.right))
				return false;

			if (!properties[i].setter (parser, config, node->assignment.right))
				return false;

			valid_property = true;
		}
	}

	if (!valid_property)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line, node->column,
								  "Unknown property '%s' in block context '%s'", identifier, block);
		return false;
	}

	return true;
}

static bool fhttpd_conf_walk (struct fhttpd_conf_parser *parser, const struct conf_node *node,
							  struct fhttpd_config *config);

static bool
fhttpd_conf_node_walk_include_file (struct fhttpd_conf_parser *parser, const struct conf_node *include_node,
									struct fhttpd_config *config, const char *fullpath)
{
	if (parser->include_fv)
	{
		for (size_t i = 0; i < parser->include_fc; i++)
		{
			if (!strcmp (parser->include_fv[i], fullpath))
			{
				fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, include_node->line,
										  include_node->column, "Recursive include of '%s'", fullpath);
				return false;
			}
		}
	}

	char **fv = realloc (parser->include_fv, sizeof (char *) * (parser->include_fc + 1));

	if (!fv)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, include_node->line, include_node->column,
								  "Memory allocation failed");
		return false;
	}

	parser->include_fv = fv;
	parser->include_fv[parser->include_fc++] = strdup (fullpath);

	struct fhttpd_conf_parser *include_parser = fhttpd_conf_parser_create_internal (fullpath, false);

	if (!include_parser)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, include_node->line, include_node->column,
								  "Failed to create parser for include file '%s'", include_node->include.filename);
		return false;
	}

	include_parser->include_fc = parser->include_fc;
	include_parser->include_fv = parser->include_fv;

	bool exists = access (fullpath, R_OK) == 0;

	if (!exists && !include_node->include.optional)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, include_node->line, include_node->column,
								  "File '%s' could not be accessed", include_node->include.filename);
		fhttpd_conf_parser_destroy (include_parser);
		return false;
	}

	if (!exists && include_node->include.optional)
	{
		fhttpd_conf_parser_destroy (include_parser);
		return true;
	}

	int rc;

	if ((rc = fhttpd_conf_parser_read (include_parser)) != 0)
	{
		fhttpd_conf_parser_error (parser, rc, include_node->line, include_node->column,
								  "Error reading include file '%s': %s", include_node->include.filename, strerror (rc));
		fhttpd_conf_parser_destroy (include_parser);
		return false;
	}

	struct conf_node *include_root = fhttpd_conf_parse (include_parser);

	parser->include_fc = include_parser->include_fc;

	if (!include_root)
	{
		parser->last_error_code = include_parser->last_error_code;
		parser->error_filename = strdup (fullpath);
		parser->error_line = include_parser->error_line;
		parser->error_column = include_parser->error_column;
		parser->last_error = strdup (include_parser->last_error);
		fhttpd_conf_parser_destroy (include_parser);
		return false;
	}

	if (!fhttpd_conf_walk (include_parser, include_root, config))
	{
		parser->last_error_code = include_parser->last_error_code;
		parser->error_filename = strdup (fullpath);
		parser->error_line = include_parser->error_line;
		parser->error_column = include_parser->error_column;
		parser->last_error = include_parser->last_error ? strdup (include_parser->last_error) : NULL;
		fhttpd_conf_free_node (include_root);
		fhttpd_conf_parser_destroy (include_parser);
		return false;
	}

	fhttpd_conf_free_node (include_root);
	fhttpd_conf_parser_destroy (include_parser);
	return true;
}

static bool
fhttpd_conf_node_walk_include_glob (struct fhttpd_conf_parser *parser, const struct conf_node *include_node,
									struct fhttpd_config *config)
{
	char fullpath[PATH_MAX + 1] = { 0 };
	glob_t glob_result;

	snprintf (fullpath, sizeof (fullpath), "%s%s%s",
			  config->conf_root != NULL && include_node->include.filename[0] != '/' ? config->conf_root : "",
			  config->conf_root == NULL || include_node->include.filename[0] == '/' ? "" : "/",
			  include_node->include.filename);

	int rc = glob (fullpath, GLOB_NOSORT | GLOB_BRACE | GLOB_ERR, NULL, &glob_result);

	if (rc == GLOB_NOMATCH)
	{
		if (include_node->include.optional)
			return true;

		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, include_node->line, include_node->column,
								  "File '%s' could not be accessed", fullpath);
		return false;
	}
	else if (rc != 0)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, include_node->line, include_node->column,
								  "Unexpected error while processing glob '%s': %s", fullpath, strerror (errno));
		return false;
	}

	for (size_t i = 0; i < glob_result.gl_pathc; i++)
	{
		char path[PATH_MAX + 1] = { 0 };

		if (!realpath (glob_result.gl_pathv[i], path))
		{
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, include_node->line,
									  include_node->column, "File '%s' could not be included: %s", fullpath,
									  strerror (errno));
			globfree (&glob_result);
			return false;
		}

		if (!fhttpd_conf_node_walk_include_file (parser, include_node, config, path))
		{
			globfree (&glob_result);
			return false;
		}
	}

	globfree (&glob_result);
	return true;
}

static bool
fhttpd_conf_handle_host_block (struct fhttpd_conf_parser *parser, const struct conf_node *block,
							   struct fhttpd_config *config)
{
	struct fhttpd_config_host *hosts = realloc (config->hosts, sizeof (*hosts) * (config->host_count + 1));

	if (!hosts)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, block->line, block->column,
								  "Memory allocation failed");
		return false;
	}

	config->hosts = hosts;
	config->host_count++;

	struct fhttpd_config_host *host = &config->hosts[config->host_count - 1];

	memset (host, 0, sizeof (*host));

	for (size_t i = 0; i < block->block.argc; i++)
	{
		const struct conf_node *arg = block->block.args[i];

		if (arg->type != CONF_NODE_LITERAL)
		{
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, arg->line, arg->column,
									  "Unexpected parameter type");
			return false;
		}

		if (arg->literal.kind != CONF_LITERAL_STRING)
		{
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, arg->line, arg->column,
									  "Parameters passed to host(...) {...} must be strings");
			return false;
		}

		char *host_entry_full = strndup (arg->literal.str_value, arg->literal.str_length);

		if (!host_entry_full)
		{
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, arg->line, arg->column,
									  "Memory allocation failed");
			return false;
		}

		const char *port_start = strchr (host_entry_full, ':');
		char *host_entry
			= port_start ? strndup (host_entry_full, port_start - host_entry_full) : strdup (host_entry_full);

		if (port_start)
		{
			if (!port_start[1])
			{
				fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, arg->line, arg->column,
										  "Invalid host '%s'", host_entry);
				free (host_entry_full);
				free (host_entry);
				return false;
			}

			struct str_split_result *ports_splitted = str_split (port_start + 1, ",");

			if (!ports_splitted)
			{
				fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, arg->line, arg->column,
										  "Memory allocation failed");
				free (host_entry_full);
				free (host_entry);
				return false;
			}

			bool err = false;

			size_t addr_index = host->bound_addr_count;
			struct fhttpd_bound_addr *addrs
				= realloc (host->bound_addrs, sizeof (*addrs) * (host->bound_addr_count + ports_splitted->count));

			if (!addrs)
			{
				fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, arg->line, arg->column,
										  "Memory allocation failed");
				free (host_entry_full);
				free (host_entry);
				err = true;
			}

			memset (addrs + host->bound_addr_count, 0, sizeof (*addrs) * (ports_splitted->count));

			host->bound_addrs = addrs;
			host->bound_addr_count += ports_splitted->count;

			for (size_t i = 0; i < ports_splitted->count; i++)
			{
				char *end = NULL;
				unsigned long port = strtoul (ports_splitted->strings[i], &end, 10);

				if (*end != 0 || port == 0 || port >= UINT16_MAX)
				{
					fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, arg->line, arg->column,
											  "Invalid port '%s'", ports_splitted->strings[i]);
					err = true;
					break;
				}

				struct fhttpd_bound_addr addr = { 0 };

				addr.hostname = strdup (host_entry);
				addr.port = (uint16_t) port;

				addrs[addr_index++] = addr;
			}

			str_split_free (ports_splitted);

			if (err)
			{
				free (host_entry_full);
				free (host_entry);
				return false;
			}
		}
		else
		{
			struct fhttpd_bound_addr *addrs
				= realloc (host->bound_addrs, sizeof (*addrs) * (host->bound_addr_count + 1));

			if (!addrs)
			{
				fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, arg->line, arg->column,
										  "Memory allocation failed");
				free (host_entry_full);
				free (host_entry);
				return false;
			}

			memset (addrs + host->bound_addr_count, 0, sizeof (*addrs));

			host->bound_addrs = addrs;
			host->bound_addr_count++;

			addrs[host->bound_addr_count - 1].port = 80;
			addrs[host->bound_addr_count - 1].hostname = strdup (host_entry);
		}

		free (host_entry_full);
		free (host_entry);
	}

	struct fhttpd_config *local_config = calloc (1, sizeof (*local_config));

	if (!local_config)
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, block->line, block->column,
								  "Memory allocation failed");
		return false;
	}

	for (size_t i = 0; i < block->block.child_count; i++)
	{
		if (!fhttpd_conf_walk (parser, block->block.children[i], local_config))
			return false;
	}

	host->host_config = local_config;
	return true;
}

static bool
fhttpd_conf_handle_block (struct fhttpd_conf_parser *parser, const struct conf_node *block,
						  struct fhttpd_config *config)
{
	const char *block_name = block->block.name->identifier.value;

	if (!strcmp (block_name, "logging"))
	{
		for (size_t i = 0; i < block->block.child_count; i++)
		{
			if (!fhttpd_conf_walk (parser, block->block.children[i], config))
				return false;
		}
	}
	else if (!strcmp (block_name, "host"))
	{
		if (!fhttpd_conf_handle_host_block (parser, block, config))
			return false;
	}
	else
	{
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, block->line, block->column,
								  "Unknown block '%s'", block_name);
		return false;
	}

	return true;
}

static bool
fhttpd_conf_walk (struct fhttpd_conf_parser *parser, const struct conf_node *node, struct fhttpd_config *config)
{
	assert (node != NULL);

	switch (node->type)
	{
		case CONF_NODE_ROOT:
			for (size_t i = 0; i < node->root.child_count; i++)
			{
				if (node->root.children[i]->type == CONF_NODE_INCLUDE)
				{
					if (!fhttpd_conf_node_walk_include_glob (parser, node->root.children[i], config))
						return false;

					continue;
				}

				if (!fhttpd_conf_walk (parser, node->root.children[i], config))
					return false;
			}

			break;

		case CONF_NODE_ASSIGNMENT:
			if (!fhttpd_conf_walk (parser, node->assignment.right, config)
				|| !fhttpd_conf_node_walk_assignment (parser, node, config))
				return false;

			break;

		case CONF_NODE_BLOCK:
			if (!fhttpd_conf_handle_block (parser, node, config))
				return false;

			break;

		case CONF_NODE_INCLUDE:
			fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line, node->column,
									  "Include statements are not supported in this context");
			return false;

		default:
			break;
	}

	return true;
}

void
fhttpd_conf_free_config (struct fhttpd_config *config)
{
	if (!config)
		return;

	if (config->hosts)
	{
		for (size_t i = 0; i < config->host_count; i++)
		{
			if (config->hosts[i].bound_addrs)
			{
				for (size_t j = 0; j < config->hosts[i].bound_addr_count; j++)
				{
					free (config->hosts[i].bound_addrs[j].hostname);
				}

				free (config->hosts[i].bound_addrs);
			}

			fhttpd_conf_free_config (config->hosts[i].host_config);
		}
	}

	free (config->docroot);
	free (config->hosts);
	free (config->conf_root);
	free (config->logging_file);
	free (config->logging_error_file);
	free (config);
}

struct fhttpd_config *
fhttpd_conf_process (struct fhttpd_conf_parser *parser)
{
	struct conf_node *root = fhttpd_conf_parse (parser);

	if (!root)
		return NULL;

	struct fhttpd_config *config = calloc (1, sizeof (*config));

	if (!config)
	{
		fhttpd_conf_free_node (root);
		fhttpd_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY, 1, 1, "Failed to allocate memory for config");
		return NULL;
	}

	if (!fhttpd_conf_walk (parser, root, config))
	{
		fhttpd_conf_free_node (root);
		fhttpd_conf_free_config (config);
		return NULL;
	}

	fhttpd_conf_free_node (root);
	return config;
}

void
fhttpd_conf_print_config (const struct fhttpd_config *config, int indent)
{
	printf ("%*sConfiguration <%p>:\n", (int) indent, "", (void *) config);

	printf ("%*ssrv_root: %s\n", (int) indent, "", config->conf_root ? config->conf_root : "(null)");
	printf ("%*sdocroot: %s\n", (int) indent, "", config->docroot ? config->docroot : "(null)");

	printf ("%*slogging.min_level: %d\n", (int) indent, "", config->logging_min_level);
	printf ("%*slogging.file: %s\n", (int) indent, "", config->logging_file ? config->logging_file : "(null)");
	printf ("%*slogging.error_file: %s\n", (int) indent, "",
			config->logging_error_file ? config->logging_error_file : "(null)");

	for (size_t i = 0; i < config->host_count; i++)
	{
		const struct fhttpd_config_host *host = &config->hosts[i];
		printf ("%*sHost [%p]:\n", (int) indent, "", (void *) host);

		for (size_t j = 0; j < host->bound_addr_count; j++)
		{
			printf ("%*s  Bound address: %s:%u\n", (int) indent, "", host->bound_addrs[j].hostname,
					host->bound_addrs[j].port);
		}

		printf ("%*s  Host Configuration:\n", (int) indent, "");
		fhttpd_conf_print_config (host->host_config, indent + 4);
		printf ("\n");
	}
}

void
fhttpd_conf_serialize (const struct fhttpd_config *config, uint8_t *outp, size_t *sizep)
{

}

void
fhttpd_conf_deserialize (const struct fhttpd_config *config, uint8_t *outp, size_t *sizep)
{
	
}