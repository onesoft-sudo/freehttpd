#ifndef FH_CORE_CONFPROC_H
#define FH_CORE_CONFPROC_H

#include "conf.h"
#include "hash/strtable.h"
#include <stdbool.h>

struct fh_traverse_ctx;

struct fh_block_handler
{
	bool (*walk_fn) (struct fh_traverse_ctx *, const struct conf_node *,
					 void *);
	bool (*is_valid_parent_fn) (struct fh_traverse_ctx *,
								const struct conf_node *,
								const struct conf_node *);
};

struct fh_traverse_ctx
{
	/* (const char *) => (struct fh_block_handler *) */
	struct strtable *block_handler_table;
	struct fh_block_handler *root_handler;
	struct fh_conf_parser *parser;
	struct fh_config_host *default_host_config;
};

bool fh_conf_traverse (struct fh_traverse_ctx *ctx,
					   const struct conf_node *node,
					   struct fh_config *config);
void fh_conf_print (struct fh_config *config, int indent);

bool fh_conf_traverse_ctx_init (struct fh_traverse_ctx *ctx,
								struct fh_conf_parser *parser);

void fh_conf_traverse_ctx_free (struct fh_traverse_ctx *ctx);
bool
fh_conf_init (struct fh_config *config);
void fh_conf_free (struct fh_config *config);

#endif /* FH_CORE_CONFPROC_H */
