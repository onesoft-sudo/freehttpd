#ifndef FHTTPD_PROTOCOL_H
#define FHTTPD_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

enum fhttpd_protocol
{
    FHTTPD_PROTOCOL_UNKNOWN,
    FHTTPD_PROTOCOL_HTTP_1_x,
    FHTTPD_PROTOCOL_H2
};

typedef enum fhttpd_protocol protocol_t;

const char *fhttpd_protocol_to_string(enum fhttpd_protocol protocol);
enum fhttpd_protocol fhttpd_string_to_protocol(const char *protocol_str);

enum fhttpd_protocol fhttpd_stream_detect_protocol(int sockfd);

#endif /* FHTTPD_PROTOCOL_H */