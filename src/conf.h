#ifndef FHTTPD_CONF_H
#define FHTTPD_CONF_H

#include <sys/socket.h>
#include <stdint.h>

#include "log.h"

enum fhttpd_config_key
{
    FHTTPD_CONFIG_PORTS,
    FHTTPD_CONFIG_WORKER_PROCESS_COUNT,
    FHTTPD_CONFIG_DOCROOT,
    FHTTPD_CONFIG_CLIENT_HEADER_TIMEOUT,
    FHTTPD_CONFIG_CLIENT_BODY_TIMEOUT,
    FHTTPD_CONFIG_RECV_TIMEOUT,
    FHTTPD_CONFIG_SEND_TIMEOUT,
    FHTTPD_CONFIG_LOG_LEVEL,
    FHTTPD_CONFIG_MAX_RESPONSE_BODY_SIZE,
    FHTTPD_CONFIG_MAX
};

enum conf_parser_error
{
	CONF_PARSER_ERROR_NONE = 0,
	CONF_PARSER_ERROR_SYNTAX_ERROR,
	CONF_PARSER_ERROR_MEMORY,
    CONF_PARSER_ERROR_INVALID_CONFIG,
};

struct fhttpd_config_server
{
    struct sockaddr_in *bound_addrs;
    size_t bound_addr_count;
};

struct fhttpd_config
{
    char *conf_root;
    enum fhttpd_log_level logging_min_level;
    char *logging_file;
    char *logging_error_file;
};

struct fhttpd_conf_parser;

struct fhttpd_conf_parser *fhttpd_conf_parser_create (const char *filename);
int fhttpd_conf_parser_read (struct fhttpd_conf_parser *parser);
void fhttpd_conf_parser_destroy (struct fhttpd_conf_parser *parser);
bool fhttpd_conf_parser_print_error (struct fhttpd_conf_parser *parser);
enum conf_parser_error fhttpd_conf_parser_last_error (struct fhttpd_conf_parser *parser);
const char *fhttpd_conf_parser_strerror (enum conf_parser_error error);
struct fhttpd_config *fhttpd_conf_process (struct fhttpd_conf_parser *parser);
void fhttpd_conf_print_config (const struct fhttpd_config *config);
void fhttpd_conf_free_config (struct fhttpd_config *config);

#endif /* FHTTPD_CONF_H */