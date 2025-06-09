#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "h2.h"
#include "log.h"

#define H2_FRAME_READ_FLAG_NO_CHECK_SETTINGS 0x1
#define H2_FRAME_READ_FLAG_IGNORE_PAYLOAD 0x2

struct h2_static_header_field
{
    const char *name;
    const char *value;
};

static struct h2_static_header_field const h2_static_headers[] = {
    { NULL, NULL },
    { ":authority", "" },
    { ":method", "GET" },
    { ":method", "POST" },
    { ":path", "/" },
    { ":path", "/index.html" },
    { ":scheme", "http" },
    { ":scheme", "https" },
    { ":status", "200" },
    { ":status", "204" },
    { ":status", "206" },
    { ":status", "304" },
    { ":status", "400" },
    { ":status", "404" },
    { ":status", "500" },
    { "accept-charset", "" },
    { "accept-encoding", "gzip, deflate" },
    { "accept-language", "" },
    { "accept-ranges", "" },
    { "accept", "" },
    { "access-control-allow-origin", "" },
    { "age", "" },
    { "allow", "" },
    { "authorization", "" },
    { "cache-control", "" },
    { "content-disposition", "" },
    { "content-encoding", "" },
    { "content-language", "" },
    { "content-length", "" },
    { "content-location", "" },
    { "content-range", "" },
    { "content-type", "" },
    { "cookie", "" },
    { "date", "" },
    { "etag", "" },
    { "expect", "" },
    { "expires", "" },
    { "from", "" },
    { "host", "" },
    { "if-match", "" },
    { "if-modified-since", "" },
    { "if-none-match", "" },
    { "if-range", "" },
    { "if-unmodified-since", "" },
    { "last-modified", "" },
    { "link", "" },
    { "location", "" },
    { "max-forwards", "" },
    { "proxy-authenticate", "" },
    { "proxy-authorization", "" },
    { "range", "" },
    { "referer", "" },
    { "refresh", "" },
    { "retry-after", "" },
    { "server", "" },
    { "set-cookie", "" },
    { "strict-transport-security", "" },
    { "transfer-encoding", "" },
    { "user-agent", "" },
    { "vary", "" },
    { "via", "" },
    { "www-authenticate", "" },
};

static const size_t h2_static_header_field_count
    = sizeof (h2_static_headers) / sizeof (h2_static_headers[0]);

static int h2_read_frame (struct h2_connection *conn, struct h2_frame *frame,
                          int flags);

struct h2_connection *
h2_connection_create (int sockfd)
{
    struct h2_connection *conn = malloc (sizeof (struct h2_connection));

    if (!conn)
        return NULL;

    conn->sockfd = sockfd;
    conn->client_stream_count = 0;
    conn->client_stream_statuses = NULL;
    conn->initial_settings_received = false;
    conn->requests = NULL;
    conn->request_count = 0;

    return conn;
}

static inline uint32_t
h2_frame_header_length (const struct h2_raw_frame_header *header)
{
    return (header->length[0] << 16) | (header->length[1] << 8)
           | header->length[2];
}

static int
h2_frame_header_send (const struct h2_frame_header *header, int sockfd)
{
    struct h2_raw_frame_header raw_header = { 0 };

    raw_header.length[0] = (header->length >> 16) & 0xFF;
    raw_header.length[1] = (header->length >> 8) & 0xFF;
    raw_header.length[2] = header->length & 0xFF;
    raw_header.type = header->type;
    raw_header.flags = header->flags;
    raw_header.stream_id = htonl (header->stream_id);

    ssize_t sent
        = send (sockfd, &raw_header, sizeof (struct h2_raw_frame_header), 0);

    if (sent < 0)
    {
        fhttpd_wclog_error ("Failed to send frame header: %s",
                            strerror (errno));
        return ERRNO_GENERIC;
    }

    return 0;
}

static inline void
h2_settings_init (const struct h2_frame_header *header,
                  struct h2_frame_settings *settings)
{
    settings->header = *header;
    settings->values[H2_SETTINGS_HEADER_TABLE_SIZE] = 4096;
    settings->values[H2_SETTINGS_ENABLE_PUSH] = 0;
    settings->values[H2_SETTINGS_MAX_CONCURRENT_STREAMS] = 100;
    settings->values[H2_SETTINGS_INITIAL_WINDOW_SIZE] = 65535;
    settings->values[H2_SETTINGS_MAX_FRAME_SIZE] = 16384;
    settings->values[H2_SETTINGS_MAX_HEADER_LIST_SIZE]
        = 128; /* 0 for this field indicates no limit */
}

static int
h2_settings_read (struct h2_frame_settings *settings, const uint8_t *buffer,
                  size_t length)
{
    if (length % 6 != 0)
    {
        fhttpd_wclog_error (
            "Invalid SETTINGS frame length %zu, must be a multiple of 6",
            length);
        return ERRNO_GENERIC;
    }

    for (size_t i = 0; i + 6 <= length; i += 6)
    {
        uint16_t setting_id = ntohs (*(uint16_t *) &buffer[i]);
        uint32_t setting_value = ntohl (*(uint32_t *) &buffer[i + 2]);

        fhttpd_wclog_debug ("Received SETTINGS entry: id=%u, value=%u",
                            setting_id, setting_value);

        if (setting_id < __H2_SETTINGS_MAX)
        {
            settings->values[setting_id] = setting_value;
        }
        else
        {
            fhttpd_wclog_warning ("Received unknown SETTINGS ID %u, ignoring",
                                  setting_id);
        }
    }

    return 0;
}

static int
h2_settings_send (const struct h2_frame_settings *settings, int sockfd)
{
    int rc;

    if ((rc = h2_frame_header_send (&settings->header, sockfd)) < 0)
    {
        fhttpd_wclog_error ("Failed to send SETTINGS frame header");
        return rc;
    }

    if (settings->header.length == 0)
        return 0;

    for (size_t i = 0; i < __H2_SETTINGS_MAX; i++)
    {
        if (settings->values[i] == 0)
            continue;

        uint16_t setting_id = htons (i);
        uint32_t setting_value = htonl (settings->values[i]);

        uint8_t setting_entry[6];

        memcpy (setting_entry, &setting_id, sizeof (setting_id));
        memcpy (setting_entry + 2, &setting_value, sizeof (setting_value));

        ssize_t sent = send (sockfd, setting_entry, sizeof (setting_entry), 0);

        if (sent < 0 || sent != sizeof (setting_entry))
        {
            fhttpd_wclog_error (
                "Failed to send SETTINGS entry: id=%u, value=%u, "
                "error: %s",
                i, settings->values[i], strerror (errno));

            return ERRNO_GENERIC;
        }
    }

    return 0;
}

static inline void
h2_settings_print (const struct h2_frame_settings *settings)
{
    fhttpd_wclog_debug ("HTTP/2 SETTINGS:");
    fhttpd_wclog_debug ("  Header Table Size: %u",
                        settings->values[H2_SETTINGS_HEADER_TABLE_SIZE]);
    fhttpd_wclog_debug ("  Enable Push: %u",
                        settings->values[H2_SETTINGS_ENABLE_PUSH]);
    fhttpd_wclog_debug ("  Max Concurrent Streams: %u",
                        settings->values[H2_SETTINGS_MAX_CONCURRENT_STREAMS]);
    fhttpd_wclog_debug ("  Initial Window Size: %u",
                        settings->values[H2_SETTINGS_INITIAL_WINDOW_SIZE]);
    fhttpd_wclog_debug ("  Max Frame Size: %u",
                        settings->values[H2_SETTINGS_MAX_FRAME_SIZE]);
    fhttpd_wclog_debug ("  Max Header List Size: %u",
                        settings->values[H2_SETTINGS_MAX_HEADER_LIST_SIZE]);
}

static int
h2_validate_settings (const struct h2_frame_settings *settings)
{
    if (settings->values[H2_SETTINGS_MAX_FRAME_SIZE]
            > H2_MAX_ALLOWED_FRAME_LENGTH
        || settings->values[H2_SETTINGS_MAX_FRAME_SIZE]
               < H2_MIN_ALLOWED_FRAME_LENGTH)
    {
        fhttpd_wclog_error (
            "SETTINGS MAX_FRAME_SIZE %u is out of range [%u, %u]",
            settings->values[H2_SETTINGS_MAX_FRAME_SIZE],
            H2_MIN_ALLOWED_FRAME_LENGTH, H2_MAX_ALLOWED_FRAME_LENGTH);
        return ERRNO_GENERIC;
    }

    if (settings->values[H2_SETTINGS_INITIAL_WINDOW_SIZE]
        > H2_MAX_ALLOWED_WINDOW_SIZE)
    {
        fhttpd_wclog_error (
            "Invalid INITIAL_WINDOW_SIZE: %u, must be between 0 and "
            "2147483647",
            settings->values[H2_SETTINGS_INITIAL_WINDOW_SIZE]);
        return ERRNO_GENERIC;
    }

    return 0;
}

static int
h2_connection_check_preface (struct h2_connection *conn)
{
    char preface[H2_PREFACE_SIZE];

    if (recv (conn->sockfd, preface, H2_PREFACE_SIZE, 0)
        < (ssize_t) H2_PREFACE_SIZE)
    {
        fhttpd_wclog_error ("Failed to receive HTTP/2 preface");
        return ERRNO_GENERIC;
    }

    if (memcmp (preface, H2_PREFACE, H2_PREFACE_SIZE) != 0)
    {
        fhttpd_wclog_error ("Invalid HTTP/2 preface received");
        return ERRNO_GENERIC;
    }

    return ERRNO_SUCCESS;
}

static int
h2_frame_header_read (struct h2_frame_header *header, int sockfd,
                      int recv_flags)
{
    struct h2_raw_frame_header raw_header = { 0 };
    ssize_t bytes_received;

    bytes_received
        = recv (sockfd, &raw_header, sizeof (raw_header), recv_flags);

    if (bytes_received < (ssize_t) sizeof (raw_header))
    {
        fhttpd_wclog_error ("Failed to read frame header: %s",
                            strerror (errno));
        return ERRNO_GENERIC;
    }

    header->length = h2_frame_header_length (&raw_header);
    header->type = raw_header.type;
    header->flags = raw_header.flags;
    header->stream_id = ntohl (raw_header.stream_id & 0x7FFFFFFF);

    return 0;
}

static int
h2_connection_exchange_settings (struct h2_connection *conn,
                                 const struct h2_frame_header *header)
{
    struct h2_frame_header client_frame_header = { 0 };
    struct h2_frame_settings client_settings = { 0 };

    if (!header
        && h2_frame_header_read (&client_frame_header, conn->sockfd, 0) < 0)
    {
        fhttpd_wclog_error ("Failed to read SETTINGS frame header");
        return ERRNO_GENERIC;
    }

    if (header)
        client_frame_header = *header;

    if (client_frame_header.type != H2_FRAME_TYPE_SETTINGS)
    {
        fhttpd_wclog_error ("Received frame type %u, expected SETTINGS frame",
                            client_frame_header.type);
        return ERRNO_GENERIC;
    }

    if (client_frame_header.stream_id != 0)
    {
        fhttpd_wclog_error ("Received SETTINGS frame with non-zero stream "
                            "ID %u, expected 0",
                            client_frame_header.stream_id);
        return ERRNO_GENERIC;
    }

    if (client_frame_header.flags & H2_FLAG_SETTINGS_ACK)
    {
        if (!conn->initial_settings_received)
        {
            fhttpd_wclog_error (
                "Received SETTINGS frame with ACK flag set, but no "
                "initial SETTINGS frame was sent");
            return ERRNO_GENERIC;
        }

        if (client_frame_header.length != 0)
        {
            fhttpd_wclog_error (
                "Received SETTINGS ACK frame with non-zero length %u, "
                "expected 0",
                client_frame_header.length);

            return ERRNO_GENERIC;
        }

        fhttpd_wclog_debug (
            "Received SETTINGS ACK frame, no further settings exchange "
            "needed");

        return 0;
    }

    if (client_frame_header.length > H2_MAX_SETTINGS_FRAME_LENGTH)
    {
        fhttpd_wclog_error (
            "Received SETTINGS frame length %u exceeds maximum %u",
            client_frame_header.length, H2_MAX_SETTINGS_FRAME_LENGTH);
        return ERRNO_GENERIC;
        ;
    }

    fhttpd_wclog_debug (
        "Received SETTINGS frame header: length=%u, type=%u, flags=%u, "
        "stream_id=%u",
        client_frame_header.length, client_frame_header.type,
        client_frame_header.flags, client_frame_header.stream_id);

    h2_settings_init (&client_frame_header, &client_settings);

    if (client_frame_header.length > 0)
    {
        uint8_t *settings_value_buffer = malloc (client_frame_header.length);

        if (recv (conn->sockfd, settings_value_buffer,
                  client_frame_header.length, 0)
            < client_frame_header.length)
        {
            fhttpd_wclog_error ("Failed to receive complete SETTINGS "
                                "frame, expected %u bytes",
                                client_frame_header.length);
            return ERRNO_GENERIC;
        }

        if (h2_settings_read (&client_settings, settings_value_buffer,
                              client_frame_header.length)
            < 0)
        {
            free (settings_value_buffer);
            return ERRNO_GENERIC;
        }

        free (settings_value_buffer);
    }

    int rc;

    if ((rc = h2_validate_settings (&client_settings)) < 0)
    {
        fhttpd_wclog_error ("Invalid SETTINGS received: %s", strerror (-rc));
        return rc;
    }

#ifndef NDEBUG
    h2_settings_print (&client_settings);
#endif

    struct h2_frame_settings server_settings_frame = { 0 };

    server_settings_frame.header.length = 0;
    server_settings_frame.header.type = H2_FRAME_TYPE_SETTINGS;
    server_settings_frame.header.flags = 0;
    server_settings_frame.header.stream_id = 0;

    memcpy (&server_settings_frame.values, &client_settings.values,
            sizeof (server_settings_frame.values));

    if ((rc = h2_settings_send (&server_settings_frame, conn->sockfd)) < 0)
    {
        fhttpd_wclog_error ("Failed to send SETTINGS frame: %s",
                            strerror (-rc));
        return rc;
    }

    fhttpd_wclog_debug ("Sent SETTINGS frame: length=%u, type=%u, flags=%u, "
                        "stream_id=%u",
                        server_settings_frame.header.length,
                        server_settings_frame.header.type,
                        server_settings_frame.header.flags,
                        server_settings_frame.header.stream_id);

    struct h2_frame_header ack_header = { 0 };

    ack_header.length = 0;
    ack_header.type = H2_FRAME_TYPE_SETTINGS;
    ack_header.flags = H2_FLAG_SETTINGS_ACK;
    ack_header.stream_id = 0;

    if ((rc = h2_frame_header_send (&ack_header, conn->sockfd)) < 0)
    {
        fhttpd_wclog_error ("Failed to send SETTINGS ACK frame: %s",
                            strerror (-rc));
        return rc;
    }

    fhttpd_wclog_debug (
        "Sent SETTINGS ACK frame: length=%u, type=%u, flags=%u, "
        "stream_id=%u",
        ack_header.length, ack_header.type, ack_header.flags,
        ack_header.stream_id);

    memcpy (&conn->settings, &server_settings_frame.values,
            sizeof (conn->settings));

    conn->initial_settings_received = true;
    return 0;
}

static int
h2_strip_priority_frames (struct h2_connection *conn)
{
    struct h2_raw_frame_header raw_header = { 0 };
    ssize_t bytes_received;
    size_t count = 0;

    while ((bytes_received
            = recv (conn->sockfd, &raw_header, sizeof (raw_header), MSG_PEEK))
           > 0)
    {
        if (bytes_received < (ssize_t) sizeof (raw_header))
        {
            fhttpd_wclog_error ("Failed to read complete frame "
                                "header, expected %zu "
                                "bytes, got %zd",
                                sizeof (raw_header), bytes_received);
            return ERRNO_GENERIC;
        }

        uint32_t length = h2_frame_header_length (&raw_header);

        if (length > conn->settings[H2_SETTINGS_MAX_FRAME_SIZE])
        {
            fhttpd_wclog_error ("Received frame length %u exceeds maximum %u",
                                length,
                                conn->settings[H2_SETTINGS_MAX_FRAME_SIZE]);
            return ERRNO_GENERIC;
        }

        if (raw_header.type == H2_FRAME_TYPE_PRIORITY)
        {
            size_t bytes_to_discard = sizeof (raw_header) + length;
            size_t discarded = 0;
            char buffer[1024];

            while (discarded < bytes_to_discard)
            {
                size_t discardable = bytes_to_discard - discarded;

                discardable = discardable < sizeof (buffer) ? discardable
                                                            : sizeof (buffer);

                ssize_t bytes = recv (conn->sockfd, buffer, discardable, 0);

                if (bytes >= 0 && (size_t) bytes < discardable)
                {
                    fhttpd_wclog_error ("Failed to discard PRIORITY frame: "
                                        "%s",
                                        strerror (errno));
                    return ERRNO_GENERIC;
                }

                discarded += bytes;
            }

            count++;
        }
        else
        {
            break;
        }
    }

    if (count)
        fhttpd_wclog_debug ("Stripped %zu PRIORITY frames from connection %d",
                            count, conn->sockfd);

    return 0;
}

static int
h2_read_frame (struct h2_connection *conn, struct h2_frame *frame, int flags)
{
    ssize_t bytes_received;
    int rc;
    struct h2_frame_header header;

    if ((rc = h2_frame_header_read (&header, conn->sockfd, 0)) < 0)
    {
        fhttpd_wclog_error ("Failed to read frame header: %s", strerror (-rc));
        return rc;
    }

    while (!(flags & H2_FRAME_READ_FLAG_NO_CHECK_SETTINGS)
           && header.stream_id == 0 && header.type == H2_FRAME_TYPE_SETTINGS)
    {
        if ((rc = h2_connection_exchange_settings (conn, &header)) < 0)
        {
            fhttpd_wclog_error ("Failed to exchange SETTINGS: %s",
                                strerror (-rc));

            return rc;
        }

        fhttpd_wclog_debug ("Received SETTINGS frame, "
                            "settings exchange complete");
        fhttpd_wclog_debug ("Reading next frame after SETTINGS");

        if ((rc = h2_frame_header_read (&header, conn->sockfd, 0)) < 0)
        {
            fhttpd_wclog_error ("Failed to read frame header: %s",
                                strerror (-rc));
            return rc;
        }
    }

    uint8_t *payload = NULL;
    size_t payload_length = 0;

    while (payload_length < header.length)
    {
        const size_t BUF_SIZE = 1024;
        size_t remaining = header.length - payload_length;
        size_t to_read = remaining < BUF_SIZE ? remaining : BUF_SIZE;
        uint8_t buffer[BUF_SIZE];

        bytes_received = recv (conn->sockfd, buffer, to_read, 0);

        if (bytes_received < 0)
        {
            rc = -errno;
            fhttpd_wclog_error ("Failed to read frame payload: %s",
                                strerror (errno));
            return rc;
        }

        if (bytes_received == 0)
        {
            break;
        }

        if (!(flags & H2_FRAME_READ_FLAG_IGNORE_PAYLOAD))
        {
            payload = realloc (payload, payload_length + bytes_received);

            if (!payload)
            {
                rc = -errno;
                fhttpd_wclog_error ("Failed to reallocate memory for "
                                    "frame payload");
                return rc;
            }

            memcpy (payload + payload_length, buffer, bytes_received);
        }

        payload_length += bytes_received;
    }

    if (payload_length < header.length)
    {
        fhttpd_wclog_error ("Received frame payload length %zu, expected %u",
                            payload_length, header.length);
        free (payload);
        return ERRNO_GENERIC;
    }

    frame->header = header;
    frame->payload = payload;
    frame->payload_length
        = (flags & H2_FRAME_READ_FLAG_IGNORE_PAYLOAD) ? 0 : payload_length;

    return ERRNO_SUCCESS;
}

static void
hexdump (const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (i % 16 == 0)
            printf ("\n%04zu: ", i);

        printf ("%02X ", buf[i]);
    }

    printf ("\n");
}

static int
h2_huffman_decode (const uint8_t *data, size_t length, char **decoded,
                   size_t *decoded_length)
{
}

static struct h2_header_field *
h2_hpack_decompress (struct h2_request *request, const uint8_t *data,
                     size_t length, int *rc)
{
    /** FIXME: Free this memory */
    struct h2_header_field *head = NULL, *tail = NULL;
    size_t i = 0;

    hexdump (data, length);

    while (i < length)
    {
        struct h2_header_field header = { 0 };

        if (data[i] & 0x80)
        {
            uint8_t static_index = data[i] & 0x7F;

            /** TODO: Add full H2 static table */
            if (static_index >= h2_static_header_field_count)
            {
                fhttpd_wclog_error ("Invalid static index %u, "
                                    "exceeds static header count "
                                    "%zu",
                                    static_index, h2_static_header_field_count);
                *rc = ERRNO_GENERIC;
                return NULL;
            }

            header.name = (char *) h2_static_headers[static_index].name;
            header.value = (char *) h2_static_headers[static_index].value;
            header.flags = H2_HEADER_FIELD_FLAG_PSEUDO
                           | H2_HEADER_FIELD_FLAG_FULL_STATIC;

            i++;
        }
        else if (data[i] & 0x40)
        {
            uint16_t dyn_index = data[i] & 0x3F;
            char *name = NULL;
            char *value = NULL;
            uint8_t flags = 0;
            bool name_from_static_table = false;

            if (dyn_index > 0)
            {
                if (dyn_index < 62)
                {
                    name_from_static_table = true;

                    if (dyn_index >= h2_static_header_field_count)
                    {
                        fhttpd_wclog_error ("Invalid static index %u, "
                                            "exceeds static header count "
                                            "%zu",
                                            dyn_index,
                                            h2_static_header_field_count);
                        *rc = ERRNO_GENERIC;
                        return NULL;
                    }
                }
                else
                {
                    dyn_index -= 62;

                    if (dyn_index >= request->dyn_table_size)
                    {
                        fhttpd_wclog_error ("Invalid dynamic index %u, "
                                            "exceeds static header count "
                                            "%zu",
                                            dyn_index,
                                            h2_static_header_field_count);
                        *rc = ERRNO_GENERIC;
                        return NULL;
                    }

                    dyn_index = request->dyn_table_size - dyn_index - 1;

                    if (dyn_index >= request->dyn_table_size)
                    {
                        fhttpd_wclog_error ("Invalid dynamic index %u, "
                                            "exceeds static header count "
                                            "%zu",
                                            dyn_index,
                                            h2_static_header_field_count);
                        *rc = ERRNO_GENERIC;
                        return NULL;
                    }
                }
            }

            i++;

            if (i >= length)
            {
                fhttpd_wclog_error (
                    "Unexpected end of data while reading header "
                    "value");
                free (name);
                *rc = ERRNO_GENERIC;
                return NULL;
            }

            if (dyn_index == 0)
            {
                size_t name_length = data[i] & 0x7F;
                bool name_is_huffman = data[i] & 0x80;

                fhttpd_log_debug (
                    "Reading dynamic header with name length %zu, "
                    "is_huffman: %s",
                    name_length, name_is_huffman ? "true" : "false");

                if (name_length > length - i - 1
                    || name_length > H2_HPACK_HEADER_NAME_MAX_LENGTH)
                {
                    fhttpd_wclog_error (
                        "Name length %zu exceeds remaining "
                        "data length %zu",
                        name_length,
                        name_length > H2_HPACK_HEADER_NAME_MAX_LENGTH
                            ? H2_HPACK_HEADER_NAME_MAX_LENGTH
                            : (length - i - 1));
                    *rc = ERRNO_GENERIC;
                    return NULL;
                }

                name = malloc (name_length + 1);

                if (!name)
                {
                    *rc = -errno;
                    fhttpd_wclog_error ("Failed to allocate memory for "
                                        "header name");
                    return NULL;
                }

                memcpy (name, data + i + 1, name_length);
                name[name_length] = 0;
                i += name_length + 1;
            }
            else
            {
                name = name_from_static_table
                           ? (char *) h2_static_headers[dyn_index].name
                           : request->dyn_table[dyn_index].name;
                flags |= H2_HEADER_FIELD_FLAG_NAME_STATIC
                         | H2_HEADER_FIELD_FLAG_NAME_INDEXED;
            }

            if (i >= length)
            {
                fhttpd_wclog_error (
                    "Unexpected end of data while reading header "
                    "value");
                free (name);
                *rc = ERRNO_GENERIC;
                return NULL;
            }

            size_t value_length = data[i] & 0x7F;
            bool value_is_huffman = data[i] & 0x80;

            fhttpd_log_debug ("Reading dynamic header with value length %zu, "
                              "is_huffman: %s",
                              value_length,
                              value_is_huffman ? "true" : "false");

            if (value_length > length - i - 1
                || value_length > H2_HPACK_HEADER_VALUE_MAX_LENGTH)
            {
                fhttpd_wclog_error ("Value length %zu exceeds remaining data "
                                    "length %zu",
                                    value_length,
                                    value_length
                                            > H2_HPACK_HEADER_VALUE_MAX_LENGTH
                                        ? H2_HPACK_HEADER_VALUE_MAX_LENGTH
                                        : (length - i - 1));
                free (name);
                *rc = ERRNO_GENERIC;
                return NULL;
            }

            value = malloc (value_length + 1);

            if (!value)
            {
                *rc = -errno;
                free (name);
                fhttpd_wclog_error (
                    "Failed to allocate memory for header value");
                return NULL;
            }

            memcpy (value, data + i + 1, value_length);
            value[value_length] = 0;

            i += value_length + 1;

            header.name = name;
            header.value = value;
            header.flags = flags;
        }
        else if (((data[i] & 0xF0) == 0 || ((data[i] & 0xF0) >> 4) == 1)
                 && i + 1 > length)
        {
            i++;

            char *name = NULL;
            char *value = NULL;

            uint8_t name_length = data[i] & 0x7F;
            bool name_is_huffman = data[i] & 0x80;

            fhttpd_log_debug (
                "Reading full indexed header with name length %u, "
                "is_huffman: %s",
                name_length, name_is_huffman ? "true" : "false");

            if (name_length > length - i - 1
                || name_length > H2_HPACK_HEADER_NAME_MAX_LENGTH)
            {
                fhttpd_wclog_error ("Name length %u exceeds remaining data "
                                    "length %zu",
                                    name_length,
                                    name_length
                                            > H2_HPACK_HEADER_NAME_MAX_LENGTH
                                        ? H2_HPACK_HEADER_NAME_MAX_LENGTH
                                        : (length - i - 1));
                *rc = ERRNO_GENERIC;
                return NULL;
            }

            name = malloc (name_length + 1);

            if (!name)
            {
                *rc = -errno;
                fhttpd_wclog_error (
                    "Failed to allocate memory for header name");
                return NULL;
            }

            memcpy (name, data + i + 1, name_length);
            name[name_length] = 0;

            i += name_length + 1;

            if (i >= length)
            {
                fhttpd_wclog_error (
                    "Unexpected end of data while reading header "
                    "value");
                free (name);
                *rc = ERRNO_GENERIC;
                return NULL;
            }

            uint8_t value_length = data[i] & 0x7F;
            bool value_is_huffman = data[i] & 0x80;

            fhttpd_log_debug (
                "Reading full indexed header with value length %u, "
                "is_huffman: %s",
                value_length, value_is_huffman ? "true" : "false");

            if (value_length > length - i - 1
                || value_length > H2_HPACK_HEADER_VALUE_MAX_LENGTH)
            {
                fhttpd_wclog_error ("Value length %u exceeds remaining data "
                                    "length %zu",
                                    value_length,
                                    value_length
                                            > H2_HPACK_HEADER_VALUE_MAX_LENGTH
                                        ? H2_HPACK_HEADER_VALUE_MAX_LENGTH
                                        : (length - i - 1));
                free (name);
                *rc = ERRNO_GENERIC;
                return NULL;
            }

            value = malloc (value_length + 1);

            if (!value)
            {
                *rc = -errno;
                free (name);
                fhttpd_wclog_error (
                    "Failed to allocate memory for header value");
                return NULL;
            }

            memcpy (value, data + i + 1, value_length);
            value[value_length] = 0;

            i += value_length + 1;

            header.name = name;
            header.value = value;
            header.flags = H2_HEADER_FIELD_FLAG_FULL_INDEXED;
        }
        else if (data[i] & 0x20)
        {
            fhttpd_wclog_error (
                "NOT IMPLEMENTED: Changes to the max dynamic table "
                "size are not supported yet");
            *rc = ERRNO_GENERIC;
            return NULL;
        }
        else
        {
            fhttpd_wclog_error ("Invalid header encoding at index %zu: 0x%02X",
                                i, data[i]);
            *rc = ERRNO_GENERIC;
            return NULL;
        }

        if (header.name == NULL || header.value == NULL)
        {
            fhttpd_wclog_error ("Header name or value is NULL");
            *rc = ERRNO_GENERIC;
            return NULL;
        }

        struct h2_header_field *field
            = malloc (sizeof (struct h2_header_field));

        if (!field)
        {
            *rc = -errno;
            fhttpd_wclog_error ("Failed to allocate memory for "
                                "header field");
            return NULL;
        }

        memcpy (field, &header, sizeof (struct h2_header_field));

        /* Insert this header field at the end of the linked list, because we
           decode headers in reverse order. */

        if (tail)
        {
            tail->next = field;
            tail = field;
        }
        else
        {
            head = field;
            tail = field;
        }

        field->next = NULL;
    }

    return head;
}

static inline size_t
h2_client_stream_id_to_index (const struct h2_connection *conn,
                              uint32_t stream_id)
{
    if (stream_id == 0)
    {
        fhttpd_wclog_error ("Invalid stream ID %u, must be odd and non-zero",
                            stream_id);
        return SIZE_MAX;
    }

    for (size_t index = 0; index < conn->client_stream_count; index++)
    {
        if (conn->client_stream_statuses[index] == stream_id)
        {
            return index;
        }
    }

    return SIZE_MAX;
}

/** FIXME: Use a hash table */
static struct h2_request *
h2_get_request_by_index (struct h2_connection *conn, uint32_t stream_id)
{
    size_t index = h2_client_stream_id_to_index (conn, stream_id);

    if (index == SIZE_MAX)
    {
        fhttpd_wclog_error ("Invalid request index %d for connection %d", index,
                            conn->sockfd);
        return NULL;
    }

    if (index + conn->client_stream_count
        >= conn->settings[H2_SETTINGS_MAX_CONCURRENT_STREAMS])
    {
        fhttpd_wclog_error (
            "Request index %d exceeds maximum concurrent streams %u for "
            "connection %d",
            index, conn->settings[H2_SETTINGS_MAX_CONCURRENT_STREAMS],
            conn->sockfd);
        return NULL;
    }

    if (index < conn->client_stream_count)
    {
        return &conn->requests[index];
    }

    fhttpd_wclog_error ("Request index %d exceeds number of requests %zu for "
                        "connection %d",
                        index, conn->client_stream_count, conn->sockfd);
    return NULL;
}

static int
h2_request_append_headers (struct h2_connection *conn,
                           const struct h2_frame *frame)
{
    bool has_padding = frame->header.flags & H2_FLAG_HEADERS_PADDED;
    bool has_priority = frame->header.flags & H2_FLAG_HEADERS_PRIORITY;
    uint8_t pad_length = 0;
    int rc;

    if (has_padding)
    {
        if (frame->payload_length < 1)
        {
            fhttpd_wclog_error (
                "Received padded HEADERS frame with no padding length");
            return ERRNO_GENERIC;
        }

        pad_length = frame->payload[0];

        if (pad_length >= frame->payload_length)
        {
            fhttpd_wclog_error ("Padding length %u exceeds payload length %zu",
                                pad_length, frame->payload_length);
            return ERRNO_GENERIC;
        }
    }

    size_t header_length = frame->payload_length - pad_length;

    if (has_priority)
    {
        if (header_length < 5)
        {
            fhttpd_wclog_error ("Received HEADERS frame with PRIORITY flag but "
                                "insufficient length for priority fields");
            return ERRNO_GENERIC;
        }

        header_length -= 5;
    }

    if (header_length == 0)
    {
        fhttpd_wclog_error ("Received HEADERS frame with no headers");
        return ERRNO_GENERIC;
    }

    struct h2_request *request
        = h2_get_request_by_index (conn, frame->header.stream_id);

    if (!request)
    {
        fhttpd_wclog_error ("Failed to get request for stream ID %u",
                            frame->header.stream_id);
        return ERRNO_GENERIC;
    }

    struct h2_header_field *header_head = h2_hpack_decompress (
        request,
        frame->payload + (has_padding ? 1 : 0) + (has_priority ? 5 : 0),
        header_length, &rc);

    if (rc < 0)
    {
        fhttpd_wclog_error ("Failed to decompress headers: %s", strerror (-rc));
        return rc;
    }

    // Print

    if (!header_head)
    {
        fhttpd_wclog_error ("No headers found in HEADERS frame");
        return ERRNO_GENERIC;
    }

    while (header_head)
    {
        fhttpd_wclog_debug ("Header: %s: %s", header_head->name,
                            header_head->value);
        header_head = header_head->next;
    }

    return ERRNO_SUCCESS;
}

/**
 * TODO: Implement actual HEADERS processing logic.
 * Currently, this function only reads frames and logs them.
 * In a real implementation, you would parse headers, handle data frames,
 * and manage stream states accordingly.
 *
 * Also, this function does not handle flow control, prioritization,
 * or TCP connection timeouts.
 */

static int
h2_connection_handle_requests (struct h2_connection *conn)
{
    while (true)
    {
        int rc;
        struct h2_frame frame = { 0 };
        bool end_stream = false, new_stream = false;

        if ((rc = h2_read_frame (conn, &frame, 0)) < 0)
        {
            fhttpd_wclog_error ("Failed to read frame: %s", strerror (-rc));
            return rc;
        }

        if (frame.header.type == H2_FRAME_TYPE_PRIORITY)
        {
            fhttpd_wclog_debug ("Received PRIORITY frame, "
                                "ignoring as per HTTP/2 spec");
            free (frame.payload);
            continue;
        }

        if (frame.header.stream_id == 0)
            goto stream_exists;

        for (size_t i = 0; i < conn->client_stream_count; i++)
        {
            if (conn->client_stream_statuses[i] == frame.header.stream_id)
            {
                fhttpd_wclog_debug ("Stream ID %u already exists, processing "
                                    "frame",
                                    frame.header.stream_id);

                goto stream_exists;
            }
        }

        if (conn->client_stream_count + 1
            >= conn->settings[H2_SETTINGS_MAX_CONCURRENT_STREAMS])
        {
            fhttpd_wclog_error (
                "Maximum concurrent streams reached (%u), "
                "closing connection",
                conn->settings[H2_SETTINGS_MAX_CONCURRENT_STREAMS]);

            free (frame.payload);
            return ERRNO_GENERIC;
        }

        uint32_t *client_stream_statuses
            = realloc (conn->client_stream_statuses,
                       sizeof (uint32_t) * (conn->client_stream_count + 1));

        if (!client_stream_statuses)
        {
            fhttpd_wclog_error ("Failed to allocate memory for "
                                "client stream statuses");
            free (frame.payload);
            return ERRNO_GENERIC;
        }

        conn->client_stream_statuses = client_stream_statuses;
        conn->client_stream_statuses[conn->client_stream_count++]
            = frame.header.stream_id;

        fhttpd_wclog_debug ("New stream ID %u added, total streams: %zu",
                            frame.header.stream_id, conn->client_stream_count);
        new_stream = true;

    stream_exists:
        if ((frame.header.type == H2_FRAME_TYPE_CONTINUATION
             || frame.header.type == H2_FRAME_TYPE_DATA
             || frame.header.type == H2_FRAME_TYPE_HEADERS)
            && frame.header.flags & H2_FLAG_END_STREAM)
        {
            end_stream = true;
            fhttpd_wclog_debug ("Received END_STREAM flag");
        }

        switch (frame.header.type)
        {
        case H2_FRAME_TYPE_DATA:
            fhttpd_wclog_debug ("Received DATA frame: length=%u, "
                                "stream_id=%u",
                                frame.header.length, frame.header.stream_id);
            break;

        case H2_FRAME_TYPE_HEADERS:
            fhttpd_wclog_debug ("Received HEADERS frame: length=%u, "
                                "stream_id=%u, flags=0x%02X",
                                frame.header.length, frame.header.stream_id,
                                frame.header.flags);

            if (new_stream)
            {
                fhttpd_wclog_info (
                    "New stream ID %u created, processing HEADERS",
                    frame.header.stream_id);

                struct h2_request *requests
                    = realloc (conn->requests, sizeof (struct h2_request)
                                                   * (conn->request_count + 1));

                if (!requests)
                {
                    fhttpd_wclog_error ("Failed to allocate memory for "
                                        "requests");
                    free (frame.payload);
                    return ERRNO_GENERIC;
                }

                conn->requests = requests;

                memset (&conn->requests[conn->request_count], 0,
                        sizeof (struct h2_request));
                conn->requests[conn->request_count].stream_id
                    = frame.header.stream_id;
                conn->requests[conn->request_count].end_stream = end_stream;
                conn->requests[conn->request_count].end_headers
                    = frame.header.flags & H2_FLAG_END_HEADERS;
            }

            if ((rc = h2_request_append_headers (conn, &frame)) < 0)
            {
                fhttpd_wclog_error ("Failed to append headers to request: %s",
                                    strerror (-rc));
                free (frame.payload);
                return rc;
            }

            break;

        case H2_FRAME_TYPE_CONTINUATION:
            fhttpd_wclog_debug ("Received CONTINUATION frame: "
                                "length=%u, stream_id=%u",
                                frame.header.length, frame.header.stream_id);
            break;

        case H2_FRAME_TYPE_PRIORITY:
            fhttpd_wclog_debug ("Received PRIORITY frame: length=%u, "
                                "stream_id=%u",
                                frame.header.length, frame.header.stream_id);
            break;

        case H2_FRAME_TYPE_RST_STREAM:
            fhttpd_wclog_debug ("Received RST_STREAM frame: length=%u, "
                                "stream_id=%u",
                                frame.header.length, frame.header.stream_id);
            break;

        case H2_FRAME_TYPE_SETTINGS:
            fhttpd_wclog_error ("Unexpected SETTINGS frame: length=%u, "
                                "stream_id=%u",
                                frame.header.length, frame.header.stream_id);
            fhttpd_log_error ("HTTP/2 SETTINGS frame received in function %s, "
                              "however it should have been handled in the read "
                              "function",
                              __func__);

            break;

        default:
            fhttpd_wclog_warning ("Received unknown frame type %u: "
                                  "length=%u, stream_id=%u",
                                  frame.header.type, frame.header.length,
                                  frame.header.stream_id);
        }

        free (frame.payload);

        if (end_stream)
        {
            fhttpd_wclog_info ("End of stream reached for stream ID %u",
                               frame.header.stream_id);

            conn->client_stream_count--;

            if (conn->client_stream_count == 0)
            {
                fhttpd_wclog_info (
                    "No more streams, not closing connection until "
                    "GOAWAY or timeout");
            }
        }
    }

    return ERRNO_SUCCESS;
}

int
h2_connection_start (struct h2_connection *conn)
{
    int rc;

    if ((rc = h2_connection_check_preface (conn)) < 0)
    {
        fhttpd_wclog_error ("Failed to check HTTP/2 preface: %s",
                            strerror (-rc));
        h2_connection_close (conn);
        return rc;
    }

    if ((rc = h2_connection_exchange_settings (conn, NULL)) < 0)
    {
        fhttpd_wclog_error ("Failed to exchange SETTINGS: %s", strerror (-rc));
        return rc;
    }

    fhttpd_wclog_info ("HTTP/2 connection established on socket %d",
                       conn->sockfd);

    if ((rc = h2_strip_priority_frames (conn)) < 0)
    {
        fhttpd_wclog_error ("Failed to strip priority frames: %s",
                            strerror (-rc));
        return rc;
    }

    return h2_connection_handle_requests (conn);
}

void
h2_connection_close (struct h2_connection *conn)
{
    if (!conn)
        return;

    if (conn->client_stream_statuses)
        free (conn->client_stream_statuses);

    close (conn->sockfd);
    free (conn);
}