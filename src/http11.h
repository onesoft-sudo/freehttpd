#ifndef FREEHTTPD_HTTP11_H
#define FREEHTTPD_HTTP11_H

#include "server.h"
#include "protocol.h"

enum http_response_code http11_parse_request(struct fhttpd_server *server, int client_sockfd, struct http_request *request);
bool http11_send_response(struct fhttpd_server *server, int client_sockfd, struct http_response *response);

#endif /* FREEHTTPD_HTTP11_H */