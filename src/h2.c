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
h2_connection_exchange_settings (struct h2_connection *conn)
{
    struct h2_frame_header client_frame_header = { 0 };
    struct h2_frame_settings client_settings = { 0 };

    if (h2_frame_header_read (&client_frame_header, conn->sockfd, 0) < 0)
        {
            fhttpd_wclog_error ("Failed to read SETTINGS frame header");
            return ERRNO_GENERIC;
        }

    if (client_frame_header.type != H2_FRAME_TYPE_SETTINGS)
        {
            fhttpd_wclog_error (
                "Received frame type %u, expected SETTINGS frame",
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
            uint8_t *settings_value_buffer
                = malloc (client_frame_header.length);

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
            fhttpd_wclog_error ("Invalid SETTINGS received: %s",
                                strerror (-rc));
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
                                        "Failed to discard PRIORITY frame: "
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

    if (!(flags & H2_FRAME_READ_FLAG_NO_CHECK_SETTINGS))
        {
            if ((rc = h2_frame_header_read (&header, conn->sockfd, MSG_PEEK))
                < 0)
                {
                    fhttpd_wclog_error ("Failed to read frame header: %s",
                                        strerror (-rc));
                    return rc;
                }

            if (header.stream_id == 0 && header.type == H2_FRAME_TYPE_SETTINGS
                && (rc = h2_connection_exchange_settings (conn)) < 0)
                {
                    fhttpd_wclog_error ("Failed to exchange SETTINGS: %s",
                                        strerror (-rc));

                    return rc;
                }
        }

    if ((rc = h2_frame_header_read (&header, conn->sockfd, 0)) < 0)
        {
            fhttpd_wclog_error ("Failed to read frame header: %s",
                                strerror (errno));

            return rc;
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
                    payload
                        = realloc (payload, payload_length + bytes_received);

                    if (!payload)
                        {
                            rc = -errno;
                            fhttpd_wclog_error (
                                "Failed to reallocate memory for "
                                "frame payload");
                            return rc;
                        }

                    memcpy (payload + payload_length, buffer, bytes_received);
                }

            payload_length += bytes_received;
        }

    if (payload_length < header.length)
        {
            fhttpd_wclog_error (
                "Received frame payload length %zu, expected %u",
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
            bool end_stream = false;

            if ((rc = h2_read_frame (conn, &frame, 0)) < 0)
                {
                    fhttpd_wclog_error ("Failed to read frame: %s",
                                        strerror (-rc));
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
                {
                    fhttpd_wclog_error ("Received frame with stream ID 0, "
                                        "which is reserved for "
                                        "connection-level frames");

                    free (frame.payload);
                    return ERRNO_GENERIC;
                }

            for (size_t i = 0; i < conn->client_stream_count; i++)
                {
                    if (conn->client_stream_statuses[i]
                        == frame.header.stream_id)
                        {
                            fhttpd_wclog_debug (
                                "Stream ID %u already exists, processing "
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
                                frame.header.stream_id,
                                conn->client_stream_count);

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
                                        frame.header.length,
                                        frame.header.stream_id);
                    break;

                case H2_FRAME_TYPE_HEADERS:
                    fhttpd_wclog_debug ("Received HEADERS frame: length=%u, "
                                        "stream_id=%u",
                                        frame.header.length,
                                        frame.header.stream_id);
                    break;

                case H2_FRAME_TYPE_CONTINUATION:
                    fhttpd_wclog_debug ("Received CONTINUATION frame: "
                                        "length=%u, stream_id=%u",
                                        frame.header.length,
                                        frame.header.stream_id);
                    break;

                case H2_FRAME_TYPE_PRIORITY:
                    fhttpd_wclog_debug ("Received PRIORITY frame: length=%u, "
                                        "stream_id=%u",
                                        frame.header.length,
                                        frame.header.stream_id);
                    break;

                case H2_FRAME_TYPE_RST_STREAM:
                    fhttpd_wclog_debug ("Received RST_STREAM frame: length=%u, "
                                        "stream_id=%u",
                                        frame.header.length,
                                        frame.header.stream_id);
                    break;

                case H2_FRAME_TYPE_SETTINGS:
                    fhttpd_wclog_error ("Unexpected SETTINGS frame: length=%u, "
                                        "stream_id=%u",
                                        frame.header.length,
                                        frame.header.stream_id);
                    fhttpd_log_error (
                        "HTTP/2 SETTINGS frame received in function %s,"
                        "however it should have been handled in the read "
                        "function",
                        __func__);

                    break;

                default:
                    fhttpd_wclog_warning ("Received unknown frame type %u: "
                                          "length=%u, stream_id=%u",
                                          frame.header.type,
                                          frame.header.length,
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