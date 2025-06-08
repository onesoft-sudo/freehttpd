#ifndef FHTTPD_H2_H
#define FHTTPD_H2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error.h"
#include "protocol.h"

#define H2_MAX_SETTINGS_FRAME_LENGTH 4096
#define H2_MAX_ALLOWED_FRAME_LENGTH 16777215
#define H2_MIN_ALLOWED_FRAME_LENGTH 16384
#define H2_MAX_ALLOWED_WINDOW_SIZE 2147483647

#define H2_FLAG_SETTINGS_ACK 0x1
#define H2_FLAG_END_STREAM 0x1
#define H2_FLAG_END_HEADERS 0x4

enum h2_frame_type
{
    H2_FRAME_TYPE_DATA = 0x0,
    H2_FRAME_TYPE_HEADERS = 0x1,
    H2_FRAME_TYPE_PRIORITY = 0x2,
    H2_FRAME_TYPE_RST_STREAM = 0x3,
    H2_FRAME_TYPE_SETTINGS = 0x4,
    H2_FRAME_TYPE_PUSH_PROMISE = 0x5,
    H2_FRAME_TYPE_PING = 0x6,
    H2_FRAME_TYPE_GOAWAY = 0x7,
    H2_FRAME_TYPE_WINDOW_UPDATE = 0x8,
    H2_FRAME_TYPE_CONTINUATION = 0x9
};

struct h2_raw_frame_header
{
    uint8_t length[3];
    uint8_t type;
    uint8_t flags;
    uint32_t stream_id;
} __attribute__ ((packed));

struct h2_frame_header
{
    uint32_t length;
    uint8_t type;
    uint8_t flags;
    uint32_t stream_id;
};

enum h2_setting_id
{
    H2_SETTINGS_HEADER_TABLE_SIZE = 0x1,
    H2_SETTINGS_ENABLE_PUSH = 0x2,
    H2_SETTINGS_MAX_CONCURRENT_STREAMS = 0x3,
    H2_SETTINGS_INITIAL_WINDOW_SIZE = 0x4,
    H2_SETTINGS_MAX_FRAME_SIZE = 0x5,
    H2_SETTINGS_MAX_HEADER_LIST_SIZE = 0x6,
    __H2_SETTINGS_MAX
};

struct h2_frame_settings
{
    struct h2_frame_header header;
    uint32_t values[__H2_SETTINGS_MAX];
};

struct h2_frame
{
    struct h2_frame_header header;
    uint8_t *payload;
    size_t payload_length;
};

struct h2_connection
{
    int sockfd;
    uint32_t settings[__H2_SETTINGS_MAX];
    bool initial_settings_received;

    uint32_t *client_stream_statuses;
    size_t client_stream_count;
};

struct h2_connection *h2_connection_create (int sockfd);
void h2_connection_close (struct h2_connection *conn);
int h2_connection_start (struct h2_connection *conn);

#endif /* FHTTPD_H2_H */