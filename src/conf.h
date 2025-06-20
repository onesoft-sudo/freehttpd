#ifndef FHTTPD_CONF_H
#define FHTTPD_CONF_H

enum fhttpd_config
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

struct fhttpd_conf_parser;

enum conf_parser_error
{
	CONF_PARSER_ERROR_NONE = 0,
	CONF_PARSER_ERROR_SYNTAX_ERROR,
	CONF_PARSER_ERROR_MEMORY_ALLOCATION
};

struct fhttpd_conf_parser *fhttpd_conf_parser_create (const char *filename);
int fhttpd_conf_parser_read (struct fhttpd_conf_parser *parser);
void fhttpd_conf_parser_destroy (struct fhttpd_conf_parser *parser);
void fhttpd_conf_parser_print_tokens (struct fhttpd_conf_parser *parser);
bool fhttpd_conf_parser_print_error (struct fhttpd_conf_parser *parser);
const char *fhttpd_conf_parser_strerror (enum conf_parser_error error);
enum conf_parser_error fhttpd_conf_parser_tokenize (struct fhttpd_conf_parser *parser);

#endif /* FHTTPD_CONF_H */