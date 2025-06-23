#ifndef FHTTPD_CONF_H
#define FHTTPD_CONF_H

#include <stdint.h>

#include "log.h"

enum conf_parser_error
{
	CONF_PARSER_ERROR_NONE = 0,
	CONF_PARSER_ERROR_SYNTAX_ERROR,
	CONF_PARSER_ERROR_MEMORY,
    CONF_PARSER_ERROR_INVALID_CONFIG,
};

struct fhttpd_bound_addr
{
    char *hostname;
    size_t hostname_len;
    char *full_hostname;
    size_t full_hostname_len;
    uint16_t port;
};

struct fhttpd_config_host
{
    struct fhttpd_bound_addr *bound_addrs;
    size_t bound_addr_count;
    struct fhttpd_config *host_config;
};

struct fhttpd_config
{
    char *conf_root;
    size_t worker_count;

    enum fhttpd_log_level logging_min_level;
    char *logging_file;
    char *logging_error_file;

    struct fhttpd_config_host *hosts;
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
struct fhttpd_config *fhttpd_conf_process (struct fhttpd_conf_parser *parser);
void fhttpd_conf_print_config (const struct fhttpd_config *config, int indent);
void fhttpd_conf_free_config (struct fhttpd_config *config);

#endif /* FHTTPD_CONF_H */
