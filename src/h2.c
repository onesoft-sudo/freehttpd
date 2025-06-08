#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "h2.h"
#include "log.h"

struct h2_connection *
h2_connection_create (int sockfd)
{
    struct h2_connection *conn = malloc (sizeof (struct h2_connection));

    if (!conn)
        return NULL;

    conn->sockfd = sockfd;
    conn->client_stream_count = 0;

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
                    fhttpd_wclog_warning (
                        "Received unknown SETTINGS ID %u, ignoring",
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

            ssize_t sent
                = send (sockfd, setting_entry, sizeof (setting_entry), 0);

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
h2_connection_exchange_settings (struct h2_connection *conn)
{
    struct h2_raw_frame_header client_raw_frame_header = { 0 };
    struct h2_frame_header client_frame_header = { 0 };
    struct h2_frame_settings client_settings = { 0 };

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

    if (recv (conn->sockfd, &client_raw_frame_header,
              sizeof (client_raw_frame_header), 0)
        < (ssize_t) sizeof (client_raw_frame_header))
        return ERRNO_GENERIC;
    ;

    if (client_raw_frame_header.type != H2_FRAME_TYPE_SETTINGS)
        {
            fhttpd_wclog_error (
                "Received frame type %u, expected SETTINGS frame",
                client_raw_frame_header.type);
            return ERRNO_GENERIC;
        }

    client_frame_header.length
        = h2_frame_header_length (&client_raw_frame_header);
    client_frame_header.stream_id = ntohl (client_raw_frame_header.stream_id);

    if (client_frame_header.stream_id != 0)
        {
            fhttpd_wclog_error ("Received SETTINGS frame with non-zero stream "
                                "ID %u, expected 0",
                                client_frame_header.stream_id);
            return ERRNO_GENERIC;
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
        client_frame_header.length, client_raw_frame_header.type,
        client_raw_frame_header.flags, client_frame_header.stream_id);

    h2_settings_init (&client_frame_header, &client_settings);

    uint8_t *settings_value_buffer = malloc (client_frame_header.length);

    if (recv (conn->sockfd, settings_value_buffer, client_frame_header.length,
              0)
        < client_frame_header.length)
        {
            fhttpd_wclog_error (
                "Failed to receive complete SETTINGS frame, expected %u bytes",
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

    if (client_settings.values[H2_SETTINGS_MAX_FRAME_SIZE]
            > H2_MAX_ALLOWED_FRAME_LENGTH
        || client_settings.values[H2_SETTINGS_MAX_FRAME_SIZE]
               < H2_MIN_ALLOWED_FRAME_LENGTH)
        {
            fhttpd_wclog_error (
                "Received SETTINGS MAX_FRAME_SIZE %u is out of range [%u, %u]",
                client_settings.values[H2_SETTINGS_MAX_FRAME_SIZE],
                H2_MIN_ALLOWED_FRAME_LENGTH, H2_MAX_ALLOWED_FRAME_LENGTH);
            return ERRNO_GENERIC;
        }

#ifndef NDEBUG
    h2_settings_print (&client_settings);
#endif

    struct h2_frame_header server_header = { 0 };

    server_header.length = 0;
    server_header.type = H2_FRAME_TYPE_SETTINGS;
    server_header.flags = 0;
    server_header.stream_id = 0;

    h2_frame_header_send (&server_header, conn->sockfd);
    fhttpd_wclog_debug ("Sent SETTINGS frame: length=%u, type=%u, flags=%u, "
                        "stream_id=%u",
                        server_header.length, server_header.type,
                        server_header.flags, server_header.stream_id);

    memcpy (&conn->settings, &client_settings.values, sizeof (conn->settings));
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
                    fhttpd_wclog_error (
                        "Failed to read complete frame header, expected %zu "
                        "bytes, got %zd",
                        sizeof (raw_header), bytes_received);
                    return ERRNO_GENERIC;
                }

            uint32_t length = h2_frame_header_length (&raw_header);

            if (length > conn->settings[H2_SETTINGS_MAX_FRAME_SIZE])
                {
                    fhttpd_wclog_error (
                        "Received frame length %u exceeds maximum %u", length,
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

                            discardable = discardable < sizeof (buffer)
                                              ? discardable
                                              : sizeof (buffer);

                            ssize_t bytes
                                = recv (conn->sockfd, buffer, discardable, 0);

                            if (bytes >= 0 && (size_t) bytes < discardable)
                                {
                                    fhttpd_wclog_error (
                                        "Failed to discard PRIORITY frame: %s",
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
        fhttpd_log_debug ("Stripped %zu PRIORITY frames from connection %d",
                          count, conn->sockfd);

    return 0;
}

int
h2_connection_start (struct h2_connection *conn)
{
    int rc;

    if ((rc = h2_connection_exchange_settings (conn)) < 0)
        {
            fhttpd_wclog_error ("Failed to exchange SETTINGS: %s",
                                strerror (-rc));
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

    /* TODO: Handle HEADERS frames, which might also include PRIORITY flag. */

    return 0;
}

void
h2_connection_close (struct h2_connection *conn)
{
    if (!conn)
        return;

    close (conn->sockfd);
    free (conn);
}