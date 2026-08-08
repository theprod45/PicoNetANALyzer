#include "telemetry.h"
#include "config.h"

#include <stdio.h>


/*
 * Print a string safely inside JSON quotes.
 */
static void json_print_string(
    const char *value)
{
    putchar('"');

    if (value != NULL)
    {
        while (*value)
        {
            unsigned char c =
                (unsigned char)*value++;

            switch (c)
            {
                case '"':
                    printf("\\\"");
                    break;

                case '\\':
                    printf("\\\\");
                    break;

                case '\n':
                    printf("\\n");
                    break;

                case '\r':
                    printf("\\r");
                    break;

                case '\t':
                    printf("\\t");
                    break;

                default:

                    if (c < 0x20)
                    {
                        printf(
                            "\\u%04x",
                            c
                        );
                    }
                    else
                    {
                        putchar(c);
                    }

                    break;
            }
        }
    }

    putchar('"');
}


static void json_print_nullable_int(
    int value)
{
    if (value < 0)
    {
        printf("null");
    }
    else
    {
        printf("%d", value);
    }
}


static void print_common_header(
    const char *type,
    uint64_t uptime_ms)
{
    printf("{");

    printf(
        "\"schema\":%d,",
        TELEMETRY_SCHEMA_VERSION
    );

    printf("\"device_id\":");
    json_print_string(DEVICE_ID);
    printf(",");

    printf("\"type\":");
    json_print_string(type);
    printf(",");

    printf(
        "\"uptime_ms\":%llu",
        (unsigned long long)uptime_ms
    );
}


void telemetry_emit_measurement(
    const telemetry_measurement_t *m)
{
    print_common_header(
        "measurement",
        m->uptime_ms
    );

    printf(",\"status\":");
    json_print_string(m->status);

    printf(",\"ssid\":");
    json_print_string(m->ssid);

    printf(",\"bssid\":");
    json_print_string(m->bssid);

    printf(
        ",\"channel\":%d",
        m->channel
    );

    printf(
        ",\"rssi_dbm\":%d",
        m->rssi_dbm
    );

    printf(",\"gateway\":");
    json_print_string(m->gateway);


    printf(",\"gateway_rtt_ms\":");
    json_print_nullable_int(
        m->gateway_rtt_ms
    );

    printf(
        ",\"gateway_loss_pct\":%.2f",
        m->gateway_loss_pct
    );


    printf(",\"internet_rtt_ms\":");
    json_print_nullable_int(
        m->internet_rtt_ms
    );


    printf(",\"min_rtt_ms\":");
    json_print_nullable_int(
        m->min_rtt_ms
    );

    printf(
        ",\"avg_rtt_ms\":%.2f",
        m->avg_rtt_ms
    );

    printf(",\"max_rtt_ms\":");
    json_print_nullable_int(
        m->max_rtt_ms
    );


    printf(",\"jitter_ms\":");
    json_print_nullable_int(
        m->jitter_ms
    );

    printf(
        ",\"avg_jitter_ms\":%.2f",
        m->avg_jitter_ms
    );


    printf(
        ",\"packet_loss_pct\":%.2f",
        m->packet_loss_pct
    );


    printf(
        ",\"samples\":%lu",
        (unsigned long)m->samples
    );

    printf(
        ",\"successful\":%lu",
        (unsigned long)m->successful
    );

    printf(
        ",\"failed\":%lu",
        (unsigned long)m->failed
    );


    printf(
        ",\"disconnects\":%lu",
        (unsigned long)m->disconnects
    );

    printf(
        ",\"reconnects\":%lu",
        (unsigned long)m->reconnects
    );

    printf(
        ",\"reconnect_attempts\":%lu",
        (unsigned long)m->reconnect_attempts
    );


    printf(
        ",\"bssid_changes\":%lu",
        (unsigned long)m->bssid_changes
    );

    printf(
        ",\"channel_changes\":%lu",
        (unsigned long)m->channel_changes
    );

    printf(
        ",\"gateway_changes\":%lu",
        (unsigned long)m->gateway_changes
    );


    printf(
        ",\"weak_signal_events\":%lu",
        (unsigned long)m->weak_signal_events
    );

    printf(
        ",\"high_latency_events\":%lu",
        (unsigned long)m->high_latency_events
    );


    printf(
        ",\"weak_signal_active\":%s",
        m->weak_signal_active
            ? "true"
            : "false"
    );

    printf(
        ",\"high_latency_active\":%s",
        m->high_latency_active
            ? "true"
            : "false"
    );


    printf("}\n");

    fflush(stdout);
}


void telemetry_emit_event_simple(
    uint64_t uptime_ms,
    const char *event,
    const char *severity)
{
    print_common_header(
        "event",
        uptime_ms
    );

    printf(",\"event\":");
    json_print_string(event);

    printf(",\"severity\":");
    json_print_string(severity);

    printf(",\"details\":{}");

    printf("}\n");

    fflush(stdout);
}


void telemetry_emit_event_change_string(
    uint64_t uptime_ms,
    const char *event,
    const char *severity,
    const char *old_value,
    const char *new_value)
{
    print_common_header(
        "event",
        uptime_ms
    );

    printf(",\"event\":");
    json_print_string(event);

    printf(",\"severity\":");
    json_print_string(severity);

    printf(",\"details\":{");

    printf("\"old\":");
    json_print_string(old_value);

    printf(",\"new\":");
    json_print_string(new_value);

    printf("}");

    printf("}\n");

    fflush(stdout);
}


void telemetry_emit_event_change_int(
    uint64_t uptime_ms,
    const char *event,
    const char *severity,
    int old_value,
    int new_value)
{
    print_common_header(
        "event",
        uptime_ms
    );

    printf(",\"event\":");
    json_print_string(event);

    printf(",\"severity\":");
    json_print_string(severity);

    printf(
        ",\"details\":{\"old\":%d,\"new\":%d}",
        old_value,
        new_value
    );

    printf("}\n");

    fflush(stdout);
}


void telemetry_emit_event_metric(
    uint64_t uptime_ms,
    const char *event,
    const char *severity,
    const char *metric,
    int value,
    int threshold)
{
    print_common_header(
        "event",
        uptime_ms
    );

    printf(",\"event\":");
    json_print_string(event);

    printf(",\"severity\":");
    json_print_string(severity);

    printf(",\"details\":{");

    printf("\"metric\":");
    json_print_string(metric);

    printf(
        ",\"value\":%d",
        value
    );

    printf(
        ",\"threshold\":%d",
        threshold
    );

    printf("}");

    printf("}\n");

    fflush(stdout);
}


void telemetry_emit_event_duration(
    uint64_t uptime_ms,
    const char *event,
    const char *severity,
    uint64_t duration_ms)
{
    print_common_header(
        "event",
        uptime_ms
    );

    printf(",\"event\":");
    json_print_string(event);

    printf(",\"severity\":");
    json_print_string(severity);

    printf(
        ",\"details\":{\"duration_ms\":%llu}",
        (unsigned long long)duration_ms
    );

    printf("}\n");

    fflush(stdout);
}
