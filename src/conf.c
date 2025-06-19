#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"

enum conf_token_type
{
	CONF_TOKEN_INVALID = 0,
	CONF_TOKEN_STRING,
	CONF_TOKEN_INT,
	CONF_TOKEN_FLOAT,
	CONF_TOKEN_BOOLEAN,
	CONF_TOKEN_IDENTIFIER,
	CONF_TOKEN_OPEN_BRACE,
	CONF_TOKEN_CLOSE_BRACE,
	CONF_TOKEN_OPEN_PARENTHESIS,
	CONF_TOKEN_CLOSE_PARENTHESIS,
	CONF_TOKEN_COMMA,
	CONF_TOKEN_SEMICOLON,
	CONF_TOKEN_EQUALS,
	CONF_TOKEN_COLON,
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

struct fhttpd_conf_parser
{
	const char *filename;
	char *source;
	size_t source_len;
	size_t line, column;
	struct conf_token *tokens;
	size_t token_count;
	char *last_error;
};

struct fhttpd_conf_parser *
fhttpd_conf_parser_create (const char *filename)
{
	struct fhttpd_conf_parser *parser = calloc (1, sizeof (*parser));

	if (!parser)
		return NULL;

	parser->filename = filename;
	parser->line = 1;
	parser->column = 1;

	return parser;
}

int
fhttpd_conf_parser_read (struct fhttpd_conf_parser *parser)
{
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
		size_t bytes_read = fread (parser->source + parser->source_len, 1, (size_t) file_size - parser->source_len, file);

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
fhttpd_conf_parser_error (struct fhttpd_conf_parser *parser, const char *message, ...)
{
	if (parser->last_error)
		free (parser->last_error);

	parser->last_error = NULL;

	va_list args;
	va_start (args, message);
	vasprintf (&parser->last_error, message, args);
	va_end (args);
}

static enum conf_parser_error
fhttpd_conf_parser_add_token (struct fhttpd_conf_parser *parser, enum conf_token_type type, const char *value,
							  size_t length)
{
	struct conf_token *tokens = realloc (parser->tokens, sizeof (struct conf_token) * (parser->token_count + 1));

	if (!tokens)
	{
		fhttpd_conf_parser_error (parser, "Failed to allocate memory for tokens");
		return CONF_PARSER_ERROR_MEMORY_ALLOCATION;
	}

	parser->tokens = tokens;
	parser->tokens[parser->token_count].type = type;
	parser->tokens[parser->token_count].value = strndup (value, length);
	parser->tokens[parser->token_count].length = length;
	parser->tokens[parser->token_count].line = parser->line;
	parser->tokens[parser->token_count].column = parser->column;
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

	free (parser->source);
	free (parser->last_error);
	free (parser);
}

enum conf_parser_error
fhttpd_conf_parser_tokenize (struct fhttpd_conf_parser *parser)
{
	size_t i = 0;
	int rc;

	while (i < parser->source_len)
	{
		char c = parser->source[i];

		if (isspace (c))
		{
			if (c == '\n')
			{
				parser->line++;
				parser->column = 1;
			}
			else
			{
				parser->column++;
			}

			i++;
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
			if ((rc = fhttpd_conf_parser_add_token (parser, token_type, &c, 1)) != CONF_PARSER_ERROR_NONE)
				return rc;

			parser->column++;
			i++;
			continue;
		}

		if (c == '"' || c == '\"')
		{
			char quote = c;
			size_t start = i;
			i++;
			parser->column++;

			while (i < parser->source_len && parser->source[i] != '"' && parser->source[i] != '\"')
			{
				if (parser->source[i] == '\n')
				{
					parser->line++;
					parser->column = 1;
				}
				else
				{
					parser->column++;
				}

				i++;
			}

			if (i >= parser->source_len || parser->source[i] != quote)
			{
				fhttpd_conf_parser_error (parser, "Unterminated string literal");
				return CONF_PARSER_ERROR_SYNTAX_ERROR;
			}

			size_t length = i - start + 1;

			if ((rc = fhttpd_conf_parser_add_token (parser, CONF_TOKEN_STRING, &parser->source[start], length))
				!= CONF_PARSER_ERROR_NONE)
				return rc;

			parser->column += length;
			i++;
			continue;
		}

		if (isdigit (c) || (c == '-' && i + 1 < parser->source_len && isdigit (parser->source[i + 1])))
		{
			size_t start = i;
			bool is_float = false;

			while (i < parser->source_len
				   && (isdigit (parser->source[i]) || parser->source[i] == '.' || parser->source[i] == '-'))
			{
				if (parser->source[i] == '\n')
				{
					parser->line++;
					parser->column = 1;
				}
				else
				{
					parser->column++;
				}

				switch (parser->source[i])
				{
					case '.':
						if (is_float)
						{
							fhttpd_conf_parser_error (parser, "Multiple decimal points in number");
							return CONF_PARSER_ERROR_SYNTAX_ERROR;
						}

						is_float = true;
						break;

					case '-':
						if (i != start)
						{
							fhttpd_conf_parser_error (parser, "Unexpected '-' in number");
							return CONF_PARSER_ERROR_SYNTAX_ERROR;
						}

						break;

					default:
						break;
				}

				i++;
			}

			size_t length = i - start;

			if ((rc = fhttpd_conf_parser_add_token (parser, is_float ? CONF_TOKEN_FLOAT : CONF_TOKEN_INT,
													&parser->source[start], length))
				!= CONF_PARSER_ERROR_NONE)
				return rc;

			continue;
		}

		if (isalpha (c) || c == '_')
		{
			size_t start = i;

			while (i < parser->source_len && (isalnum (parser->source[i]) || parser->source[i] == '_'))
			{
				if (parser->source[i] == '\n')
				{
					parser->line++;
					parser->column = 1;
				}
				else
				{
					parser->column++;
				}

				i++;
			}

			size_t length = i - start;

			if ((rc = fhttpd_conf_parser_add_token (parser, CONF_TOKEN_IDENTIFIER, &parser->source[start], length))
				!= CONF_PARSER_ERROR_NONE)
				return rc;

			continue;
		}

		fhttpd_conf_parser_error (parser, "Unexpected character '%c'", c);
		return CONF_PARSER_ERROR_SYNTAX_ERROR;
	}

	if ((rc = fhttpd_conf_parser_add_token (parser, CONF_TOKEN_IDENTIFIER, "[EOF]", 5)) != CONF_PARSER_ERROR_NONE)
		return rc;

	return CONF_PARSER_ERROR_NONE;
}

void
fhttpd_conf_parser_print_tokens (struct fhttpd_conf_parser *parser)
{
	for (size_t i = 0; i < parser->token_count; i++)
	{
		struct conf_token *token = &parser->tokens[i];

		printf ("[%zu]: Token <type=%d, value='%.*s', line=%zu, column=%zu>\n", i, token->type, (int) token->length,
				token->value, token->line, token->column);
	}
}

bool
fhttpd_conf_parser_print_error (struct fhttpd_conf_parser *parser)
{
	if (parser->last_error)
		fprintf (stderr, "%s:%zu:%zu: %s\n", parser->filename, parser->line, parser->column, parser->last_error);

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
		case CONF_PARSER_ERROR_MEMORY_ALLOCATION:
			return "Memory allocation error";
		default:
			return "Unknown error";
	}
}