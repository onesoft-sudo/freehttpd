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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <limits.h>

#define CORE_CONF_IMPLEMENTATION

#include "conf.h"
#include "confproc.h"

static bool fh_conf_traverse_block (struct fh_traverse_ctx *ctx,
									const struct conf_node *node, void *config);

static bool
fh_conf_expect_value (struct fh_traverse_ctx *ctx, const struct conf_node *node,
					  enum conf_literal_kind kind)
{
	if (node->type != CONF_NODE_LITERAL)
	{
		fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG,
								  node->line, node->column,
								  "Expected a literal value");
		return false;
	}

	if (node->details.literal.kind != kind)
	{
		fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG,
								  node->line, node->column, "Expected %s value",
								  kind == CONF_LITERAL_BOOLEAN	? "boolean"
								  : kind == CONF_LITERAL_FLOAT	? "float"
								  : kind == CONF_LITERAL_INT	? "integer"
								  : kind == CONF_LITERAL_STRING ? "string"
																: "unknown");
		return false;
	}

	return true;
}

static bool
fh_conf_traverse_include_file (struct fh_traverse_ctx *ctx,
									const struct conf_node *include_node,
									struct fh_config *root_config,
									const char *fullpath)
{
    struct fh_conf_parser *parser = ctx->parser;

	if (parser->include_fv)
	{
		for (size_t i = 0; i < parser->include_fc; i++)
		{
			if (!strcmp (parser->include_fv[i], fullpath))
			{
				fh_conf_parser_error (
					parser, CONF_PARSER_ERROR_INVALID_CONFIG,
					include_node->line, include_node->column,
					"Recursive include of '%s'", fullpath);
				return false;
			}
		}
	}

	char **fv = realloc (parser->include_fv,
						 sizeof (char *) * (parser->include_fc + 1));

	if (!fv)
	{
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_MEMORY,
								  include_node->line, include_node->column,
								  "Memory allocation failed");
		return false;
	}

	parser->include_fv = fv;
	parser->include_fv[parser->include_fc++] = strdup (fullpath);

	struct fh_conf_parser *include_parser
		= fh_conf_parser_create_internal (fullpath, false);

	if (!include_parser)
	{
		fh_conf_parser_error (
			parser, CONF_PARSER_ERROR_MEMORY, include_node->line,
			include_node->column,
			"Failed to create parser for include file '%s'",
			include_node->details.include.filename);
		return false;
	}

	include_parser->include_fc = parser->include_fc;
	include_parser->include_fv = parser->include_fv;

	bool exists = access (fullpath, R_OK) == 0;

	if (!exists && !include_node->details.include.optional)
	{
		fh_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG,
								  include_node->line, include_node->column,
								  "File '%s' could not be accessed",
								  include_node->details.include.filename);
		fh_conf_parser_destroy (include_parser);
		return false;
	}

	if (!exists && include_node->details.include.optional)
	{
		fh_conf_parser_destroy (include_parser);
		return true;
	}

	int rc;

	if ((rc = fh_conf_parser_read (include_parser)) != 0)
	{
		fh_conf_parser_error (
			parser, rc, include_node->line, include_node->column,
			"Error reading include file '%s': %s",
			include_node->details.include.filename, strerror (rc));
		fh_conf_parser_destroy (include_parser);
		return false;
	}

	struct fh_config *config = fh_conf_process (include_parser, ctx, root_config);

	parser->include_fc = include_parser->include_fc;

	if (!config)
	{
		parser->error_filename = strdup (fullpath);
		parser->last_error_code = include_parser->last_error_code;
		parser->error_line = include_parser->error_line;
		parser->error_column = include_parser->error_column;
		parser->last_error = include_parser->last_error ? strdup (include_parser->last_error) : parser->last_error;
		fh_conf_parser_destroy (include_parser);
		return false;
	}

	fh_conf_parser_destroy (include_parser);
	return true;
}

static bool
fh_conf_traverse_include_glob (struct fh_traverse_ctx *ctx,
									const struct conf_node *include_node,
									struct fh_config *root_config)
{
    struct fh_conf_parser *parser = ctx->parser;

	char fullpath[PATH_MAX + 1] = { 0 };
	glob_t glob_result;

	snprintf (fullpath, sizeof (fullpath), "%s%s%s",
			  root_config->conf_root != NULL
					  && include_node->details.include.filename[0] != '/'
				  ? root_config->conf_root
				  : "",
			  root_config->conf_root == NULL
					  || include_node->details.include.filename[0] == '/'
				  ? ""
				  : "/",
			  include_node->details.include.filename);

	int rc = glob (fullpath, GLOB_NOSORT | GLOB_BRACE | GLOB_ERR, NULL,
				   &glob_result);

	if (rc == GLOB_NOMATCH)
	{
		if (include_node->details.include.optional)
			return true;

		fh_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG,
								  include_node->line, include_node->column,
								  "File '%s' could not be accessed", fullpath);
		return false;
	}
	else if (rc != 0)
	{
		fh_conf_parser_error (
			parser, CONF_PARSER_ERROR_INVALID_CONFIG, include_node->line,
			include_node->column,
			"Unexpected error while processing glob '%s': %s", fullpath,
			strerror (errno));
		return false;
	}

	for (size_t i = 0; i < glob_result.gl_pathc; i++)
	{
		char path[PATH_MAX + 1] = { 0 };

		if (!realpath (glob_result.gl_pathv[i], path))
		{
			fh_conf_parser_error (parser, CONF_PARSER_ERROR_INVALID_CONFIG,
									  include_node->line, include_node->column,
									  "File '%s' could not be included: %s",
									  fullpath, strerror (errno));
			globfree (&glob_result);
			return false;
		}

		if (!fh_conf_traverse_include_file (ctx, include_node, root_config,
												 path))
		{
			globfree (&glob_result);
			return false;
		}
	}

	globfree (&glob_result);
	return true;
}

static bool
fh_conf_traverse_root_block_assignment (struct fh_traverse_ctx *ctx,
										const struct conf_node *node,
										struct fh_config *config)
{
	const char *prop_name
		= node->details.assignment.left->details.identifier.value;

	if (!strcmp (prop_name, "root"))
	{
		if (!fh_conf_expect_value (ctx, node->details.assignment.right,
								   CONF_LITERAL_STRING))
			return false;

		if (config->conf_root)
			free (config->conf_root);

		config->conf_root = strdup (
			node->details.assignment.right->details.literal.value.str.value);
	}
	else if (!strcmp (prop_name, "worker_count"))
	{
		if (!fh_conf_expect_value (ctx, node->details.assignment.right,
								   CONF_LITERAL_INT))
			return false;

		int64_t value
			= node->details.assignment.right->details.literal.value.int_value;

		if (value <= 0)
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG,
				node->details.assignment.right->line,
				node->details.assignment.right->column,
				"Expected a positive non-zero integer value");
			return false;
		}

		config->worker_count = (size_t) value;
	}
	else
	{
		fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG,
								  node->details.assignment.left->line,
								  node->details.assignment.left->column,
								  "Invalid property '%s'", prop_name);
		return false;
	}

	return true;
}

static bool
fh_conf_traverse_root_block (struct fh_traverse_ctx *ctx,
							 const struct conf_node *node, void *src_config)
{
	struct fh_config *config = src_config;

	for (size_t i = 0; i < node->details.root.child_count; i++)
	{
		struct conf_node *child = node->details.root.children[i];

		switch (child->type)
		{
			case CONF_NODE_ASSIGNMENT:
				if (!fh_conf_traverse_root_block_assignment (ctx, child,
															 config))
					return false;

				break;

			case CONF_NODE_BLOCK:
			case CONF_NODE_INCLUDE:
				break;

			default:
				fh_conf_parser_error (
					ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, child->line,
					child->column,
					"Syntax error: unexpected junk (expected only properties)");
				return false;
		}
	}

	return true;
}

static bool
fh_conf_traverse_require_root_or_host_parent (struct fh_traverse_ctx *ctx,
											  const struct conf_node *node,
											  const struct conf_node *parent)
{
	(void) ctx;
	(void) node;

	return !parent
		   || (parent->type == CONF_NODE_BLOCK
			   && !strcmp (parent->details.block.name->details.identifier.value,
						   "host"));
}

static bool
fh_conf_traverse_require_no_parent (struct fh_traverse_ctx *ctx,
									const struct conf_node *node,
									const struct conf_node *parent)
{
	(void) ctx;
	(void) node;

	return !parent;
}

static bool
fh_conf_traverse_generic_block_block (struct fh_traverse_ctx *ctx,
									  const struct conf_node *node)
{
	fh_conf_parser_error (
		ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line, node->column,
		"Invalid block '%s' in %s",
		node->details.block.name->details.identifier.value,
		node->parent
			? node->parent->details.block.name->details.identifier.value
			: "root");

	return false;
}

/* logging block */

static bool
fh_conf_traverse_logging_block_assignment (struct fh_traverse_ctx *ctx,
										   const struct conf_node *node,
										   struct fh_config_logging *config)
{
	const char *prop_name
		= node->details.assignment.left->details.identifier.value;
	const struct conf_node *value = node->details.assignment.right;

	if (!strcmp (prop_name, "enabled"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_BOOLEAN))
			return false;

		config->enabled = value->details.literal.value.bool_value;
	}
	else if (!strcmp (prop_name, "min_level"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_STRING))
			return false;

		const char *strval = value->details.literal.value.str.value;
		int min_level = !strcmp (strval, "debug")		? LOG_DEBUG
						: !strcmp (strval, "info")		? LOG_INFO
						: !strcmp (strval, "warn")		? LOG_WARN
						: !strcmp (strval, "error")		? LOG_ERR
						: !strcmp (strval, "emergency") ? LOG_EMERG
														: -1;

		if (min_level == -1)
		{
			fh_conf_parser_error (ctx->parser,
									  CONF_PARSER_ERROR_INVALID_CONFIG,
									  value->line, value->column,
									  "Value of `min_level' must be one of: "
									  "debug, info, warn, error, emergency");
			return false;
		}

		config->min_level = min_level;
	}
	else if (!strcmp (prop_name, "file"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_STRING))
			return false;

		const char *strval = value->details.literal.value.str.value;

		if (config->file)
			free (config->file);

		config->file = strdup (strval);
	}
	else if (!strcmp (prop_name, "error_file"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_STRING))
			return false;

		const char *strval = value->details.literal.value.str.value;

		if (config->error_file)
			free (config->error_file);

		config->error_file = strdup (strval);
	}
	else
	{
		fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG,
								  node->details.assignment.left->line,
								  node->details.assignment.left->column,
								  "Invalid property '%s' in block 'logging'",
								  prop_name);
		return false;
	}

	return true;
}

static bool
fh_conf_traverse_logging_block (struct fh_traverse_ctx *ctx,
								const struct conf_node *node, void *src_config)
{
	struct fh_config_logging *logging;

	if (node->parent)
	{
		if (node->parent->type != CONF_NODE_BLOCK)
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line,
				node->column, "Invalid block 'logging'");

			return false;
		}

		if (strcmp (node->parent->details.block.name->details.identifier.value,
					"host"))
		{
			if (!fh_conf_traverse_generic_block_block (ctx, node))
				return false;
		}

		logging = ((struct fh_config_host *) src_config)->logging;

		if (!logging)
		{
			logging = calloc (1, sizeof (*logging));

			if (!logging)
			{
				fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_MEMORY,
										  node->line, node->column,
										  "Memory allocation error");
				return false;
			}

			((struct fh_config_host *) src_config)->logging = logging;
		}
	}
	else
	{
		logging = ((struct fh_config *) src_config)->logging;

		if (!logging)
		{
			logging = calloc (1, sizeof (*logging));

			if (!logging)
			{
				fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_MEMORY,
										  node->line, node->column,
										  "Memory allocation error");
				return false;
			}

			((struct fh_config *) src_config)->logging = logging;
		}
	}

	for (size_t i = 0; i < node->details.block.child_count; i++)
	{
		struct conf_node *child = node->details.block.children[i];

		switch (child->type)
		{
			case CONF_NODE_ASSIGNMENT:
				if (!fh_conf_traverse_logging_block_assignment (ctx, child,
																logging))
					return false;

				break;

			default:
				fh_conf_parser_error (
					ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, child->line,
					child->column,
					"Syntax error: unexpected junk (expected only properties)");
				return false;
		}
	}

	return true;
}

/* end logging block */

/* security block */

static bool
fh_conf_traverse_security_block_assignment (struct fh_traverse_ctx *ctx,
											const struct conf_node *node,
											struct fh_config_security *config)
{
	const char *prop_name
		= node->details.assignment.left->details.identifier.value;
	const struct conf_node *value = node->details.assignment.right;

	if (!strcmp (prop_name, "max_response_body_size"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_INT))
			return false;

		int64_t intval = value->details.literal.value.int_value;

		if (intval <= 0)
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line,
				value->column, "Expected a positive non-zero integer value");
			return false;
		}

		config->max_response_body_size = (size_t) intval;
	}
	else if (!strcmp (prop_name, "max_connections"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_INT))
			return false;

		int64_t intval = value->details.literal.value.int_value;

		if (intval <= 0)
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line,
				value->column, "Expected a positive non-zero integer value");
			return false;
		}

		config->max_connections = (size_t) intval;
	}
	else if (!strcmp (prop_name, "body_timeout"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_INT))
			return false;

		int64_t intval = value->details.literal.value.int_value;

		if (intval <= 0)
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line,
				value->column, "Expected a positive non-zero integer value");
			return false;
		}

		config->body_timeout = (uint64_t) intval;
	}
	else if (!strcmp (prop_name, "header_timeout"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_INT))
			return false;

		int64_t intval = value->details.literal.value.int_value;

		if (intval <= 0)
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line,
				value->column, "Expected a positive non-zero integer value");
			return false;
		}

		config->header_timeout = (uint64_t) intval;
	}
	else if (!strcmp (prop_name, "recv_timeout"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_INT))
			return false;

		int64_t intval = value->details.literal.value.int_value;

		if (intval <= 0)
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line,
				value->column, "Expected a positive non-zero integer value");
			return false;
		}

		config->recv_timeout = (uint64_t) intval;
	}
	else if (!strcmp (prop_name, "send_timeout"))
	{
		if (!fh_conf_expect_value (ctx, value, CONF_LITERAL_INT))
			return false;

		int64_t intval = value->details.literal.value.int_value;

		if (intval <= 0)
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, value->line,
				value->column, "Expected a positive non-zero integer value");
			return false;
		}

		config->send_timeout = (uint64_t) intval;
	}
	else
	{
		fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG,
								  node->details.assignment.left->line,
								  node->details.assignment.left->column,
								  "Invalid property '%s' in block 'security'",
								  prop_name);
		return false;
	}

	return true;
}

static bool
fh_conf_traverse_security_block (struct fh_traverse_ctx *ctx,
								 const struct conf_node *node, void *src_config)
{
	struct fh_config *config = src_config;

	for (size_t i = 0; i < node->details.block.child_count; i++)
	{
		struct conf_node *child = node->details.block.children[i];

		switch (child->type)
		{
			case CONF_NODE_ASSIGNMENT:
				if (!fh_conf_traverse_security_block_assignment (
						ctx, child, config->security))
					return false;

				break;

			default:
				fh_conf_parser_error (
					ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, child->line,
					child->column,
					"Syntax error: unexpected junk (expected only properties)");
				return false;
		}
	}

	return true;
}

/* end security block */

static bool
fh_conf_traverse_host_block_assignment (struct fh_traverse_ctx *ctx,
										const struct conf_node *node,
										struct fh_config_host *host)
{
	const char *prop_name
		= node->details.assignment.left->details.identifier.value;

	if (!strcmp (prop_name, "is_default"))
	{
		if (!fh_conf_expect_value (ctx, node->details.assignment.right,
								   CONF_LITERAL_BOOLEAN))
			return false;

		bool is_default
			= node->details.assignment.right->details.literal.value.bool_value;

		if (ctx->default_host_config && is_default)
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line,
				node->column, "A default host is already set");

			return false;
		}

		host->is_default = is_default;

		if (is_default)
			ctx->default_host_config = host;
		else if (!is_default && ctx->default_host_config == host)
			ctx->default_host_config = NULL;
	}
	else if (!strcmp (prop_name, "docroot"))
	{
		if (!fh_conf_expect_value (ctx, node->details.assignment.right,
								   CONF_LITERAL_STRING))
			return false;

		if (host->docroot)
			free (host->docroot);

		host->docroot = strdup (
			node->details.assignment.right->details.literal.value.str.value);
	}
	else
	{
		fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG,
								  node->details.assignment.left->line,
								  node->details.assignment.left->column,
								  "Invalid property '%s' in block 'host'",
								  prop_name);
		return false;
	}

	return true;
}

static bool
fh_conf_traverse_host_block (struct fh_traverse_ctx *ctx,
							 const struct conf_node *node, void *src_config)
{
	struct fh_config *config = src_config;

	if (!config->hosts)
	{
		config->hosts = strtable_create (0);

		if (!config->hosts)
		{
			fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_MEMORY,
									  node->line, node->column,
									  "Memory allocation error");
			return false;
		}
	}

	if (node->details.block.argc == 0)
	{
		fh_conf_parser_error (
			ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line,
			node->column, "Expected at least one argument, but none given");
		return false;
	}

	for (size_t i = 0; i < node->details.block.argc; i++)
	{
		struct conf_node *arg = node->details.block.args[i];

		if (!fh_conf_expect_value (ctx, arg, CONF_LITERAL_STRING))
			return false;

		const char *host_value = arg->details.literal.value.str.value;

		if (strtable_contains (config->hosts, host_value))
		{
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, arg->line,
				arg->column,
				"Host '%s' is already defined, refusing to overwrite",
				host_value);
			return false;
		}

		struct fh_config_host *host = calloc (1, sizeof (*host));

		if (!host)
		{
			fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_MEMORY,
									  node->line, node->column,
									  "Memory allocation error");
			return false;
		}

		strtable_set (config->hosts, host_value, host);

		host->addr.full_hostname = strdup (host_value);
		host->addr.full_hostname_len = strlen (host_value);

		char *colon = strchr (host_value, ':');

		if (!colon)
		{
			host->addr.hostname = strdup (host_value);
			host->addr.hostname_len = host->addr.full_hostname_len;
			host->addr.port = 80;
		}
		else
		{
			if (colon[1] == 0)
			{
				fh_conf_parser_error (
					ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, arg->line,
					arg->column, "No port specified after ':'");
				return false;
			}

			char *end = NULL;
			unsigned long port = strtoul (colon + 1, &end, 10);

			if (!end || *end || port >= UINT16_MAX)
			{
				fh_conf_parser_error (
					ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, arg->line,
					arg->column, "Invalid port '%s'", colon + 1);
				return false;
			}

			host->addr.port = (uint16_t) port;
			host->addr.hostname_len = (size_t) (colon - host_value);
			host->addr.hostname = strndup (host_value, host->addr.hostname_len);
		}

		if (!host->logging)
		{
			host->logging = calloc (1, sizeof (*host->logging));

			if (!host->logging)
			{
				fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_MEMORY,
										  node->line, node->column,
										  "Memory allocation error");
				return false;
			}
		}

		for (size_t i = 0; i < node->details.block.child_count; i++)
		{
			struct conf_node *child = node->details.block.children[i];

			switch (child->type)
			{
				case CONF_NODE_ASSIGNMENT:
					if (!fh_conf_traverse_host_block_assignment (ctx, child,
																 host))
						return false;

					break;

				case CONF_NODE_BLOCK:
					if (!fh_conf_traverse_block (ctx, child, host))
						return false;

					break;

				default:
					fh_conf_parser_error (ctx->parser,
											  CONF_PARSER_ERROR_INVALID_CONFIG,
											  child->line, child->column,
											  "Syntax error: unexpected junk "
											  "(expected only properties)");
					return false;
			}
		}
	}

	return true;
}

bool
fh_conf_traverse_ctx_init (struct fh_traverse_ctx *ctx,
						   struct fh_conf_parser *parser)
{
	ctx->parser = parser;
	ctx->block_handler_table = strtable_create (0);

	if (!ctx->block_handler_table)
		return false;

	struct fh_block_handler *handlers
		= calloc (4, sizeof (struct fh_block_handler));

	if (!handlers)
	{
		strtable_destroy (ctx->block_handler_table);
		return false;
	}

	handlers[0].walk_fn = &fh_conf_traverse_root_block;
	handlers[1].walk_fn = &fh_conf_traverse_host_block;
	handlers[1].is_valid_parent_fn = &fh_conf_traverse_require_no_parent;
	handlers[2].walk_fn = &fh_conf_traverse_logging_block;
	handlers[2].is_valid_parent_fn
		= &fh_conf_traverse_require_root_or_host_parent;
	handlers[3].walk_fn = &fh_conf_traverse_security_block;
	handlers[3].is_valid_parent_fn = &fh_conf_traverse_require_no_parent;

	if (!strtable_set (ctx->block_handler_table, "host", &handlers[1]))
	{
		free (handlers);
		strtable_destroy (ctx->block_handler_table);
		return false;
	}

	if (!strtable_set (ctx->block_handler_table, "logging", &handlers[2]))
	{
		free (handlers);
		strtable_destroy (ctx->block_handler_table);
		return false;
	}

	if (!strtable_set (ctx->block_handler_table, "security", &handlers[3]))
	{
		free (handlers);
		strtable_destroy (ctx->block_handler_table);
		return false;
	}

	ctx->root_handler = &handlers[0];
	return true;
}

void
fh_conf_traverse_ctx_free (struct fh_traverse_ctx *ctx)
{
	free (ctx->root_handler);
	strtable_destroy (ctx->block_handler_table);
}

static bool
fh_conf_traverse_block (struct fh_traverse_ctx *ctx,
						const struct conf_node *node, void *config)
{
	const char *name = node->details.block.name->details.identifier.value;
	struct fh_block_handler *handler
		= strtable_get (ctx->block_handler_table, name);

	if (!handler)
	{
		fh_conf_parser_error (ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG,
								  node->line, node->column,
								  "Invalid block '%s'", name);
		return false;
	}

	if (handler->is_valid_parent_fn
		&& !handler->is_valid_parent_fn (ctx, node, node->parent))
	{
		fh_conf_parser_error (
			ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line,
			node->column, "Invalid block '%s' inside '%s'", name,
			node->parent
				? node->parent->details.block.name->details.identifier.value
				: "root");

		return false;
	}

	return handler->walk_fn (ctx, node, config);
}

bool
fh_conf_init (struct fh_config *config)
{
	if (!config->logging)
	{
		config->logging = calloc (1, sizeof (*config->logging));

		if (!config->logging)
		{
			return false;
		}
	}

	if (!config->security)
	{
		config->security = calloc (1, sizeof (*config->security));

		if (!config->security)
		{
			return false;
		}
	}

	return true;
}

bool
fh_conf_traverse (struct fh_traverse_ctx *ctx, const struct conf_node *node,
				  struct fh_config *config)
{
	switch (node->type)
	{
		case CONF_NODE_ROOT:
            if (!fh_conf_traverse_root_block (ctx, node, config))
                return false;

			for (size_t i = 0; i < node->details.root.child_count; i++)
			{
                struct conf_node *child = node->details.root.children[i];

                if (child->type == CONF_NODE_INCLUDE)
                {
                    if (!fh_conf_traverse_include_glob (ctx, child, config))
                        return false;
                }
				else if (!fh_conf_traverse (ctx, child,
									   config))
					return false;
			}

			config->default_host_config = ctx->default_host_config;
			return true;

		case CONF_NODE_BLOCK:
			return fh_conf_traverse_block (ctx, node, config);

		case CONF_NODE_ASSIGNMENT:
			return true;

		default:
			fh_conf_parser_error (
				ctx->parser, CONF_PARSER_ERROR_INVALID_CONFIG, node->line,
				node->column, "Invalid node: %d", node->type);
			return false;
	}

	return true;
}

static void
fh_conf_print_security (struct fh_config_security *security, int indent)
{
	fh_pr_debug ("%*sBlock [security] <%p>:", indent, "", (void *) security);
	fh_pr_debug ("%*smax_response_body_size = %zu", indent + 2, "",
				 security->max_response_body_size);
	fh_pr_debug ("%*smax_connections = %zu", indent + 2, "",
				 security->max_connections);
	fh_pr_debug ("%*sheader_timeout = %u", indent + 2, "",
				 security->header_timeout);
	fh_pr_debug ("%*sbody_timeout = %u", indent + 2, "",
				 security->body_timeout);
	fh_pr_debug ("%*srecv_timeout = %u", indent + 2, "",
				 security->recv_timeout);
	fh_pr_debug ("%*ssend_timeout = %u", indent + 2, "",
				 security->send_timeout);
}

static void
fh_conf_print_logging (struct fh_config_logging *logging, int indent)
{
	fh_pr_debug ("%*sBlock [logging] <%p>:", indent, "", (void *) logging);
	fh_pr_debug ("%*senabled = %s", indent + 2, "",
				 logging && logging->enabled ? "true" : "false");
	fh_pr_debug ("%*smin_level = %d", indent + 2, "",
				 logging ? logging->min_level : 0);
	fh_pr_debug ("%*sfile = %s", indent + 2, "",
				 logging ? logging->file : "[default]");
	fh_pr_debug ("%*serror_file = %s", indent + 2, "",
				 logging ? logging->error_file : "[default]");
}

void
fh_conf_print (struct fh_config *config, int indent)
{
	fh_pr_debug ("%*sConfiguration <%p>:", indent, "", (void *) config);
	fh_pr_debug ("%*sroot = %s", indent, "", config->conf_root);
	fh_pr_debug ("%*sworker_count = %zu", indent, "", config->worker_count);

	for (struct strtable_entry *entry = config->hosts->head; entry;
		 entry = entry->next)
	{
		struct fh_config_host *host = entry->data;
		fh_pr_debug (
			"%*sBlock [host] <%p, key:\"%s\", host:\"%s\", port:\"%u\"%s>:",
			indent, "", (void *) host, host->addr.full_hostname,
			host->addr.hostname, host->addr.port,
			host == config->default_host_config ? ", default:1" : "");
		fh_pr_debug ("%*sis_default = %s", indent + 2, "",
					 host->is_default ? "true" : "false");
		fh_pr_debug ("%*sdocroot = %s", indent + 2, "", host->docroot);
		fh_conf_print_logging (host->logging, indent + 2);
	}

	fh_conf_print_logging (config->logging, indent);
	fh_conf_print_security (config->security, indent);
}

void
fh_conf_free_logging_config (struct fh_config_logging *logging)
{
	free (logging->file);
	free (logging->error_file);
	free (logging);
}

void
fh_conf_free (struct fh_config *config)
{
	if (!config)
		return;

    if (config->logging)
	    fh_conf_free_logging_config (config->logging);

	free (config->security);

	if (!config->no_free_hosts && config->hosts)
	{
		for (struct strtable_entry *entry = config->hosts->head; entry;
			 entry = entry->next)
		{
			struct fh_config_host *host = entry->data;

            if (host->logging)
			    fh_conf_free_logging_config (host->logging);

            free (host->docroot);
			free (host->addr.full_hostname);
			free (host->addr.hostname);
            free (host);
		}

		strtable_destroy (config->hosts);
	}

	free (config->conf_root);
	free (config);
}
