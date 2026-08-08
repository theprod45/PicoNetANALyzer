#include "telemetry.h"

#include <stdio.h>

#include "config.h"


/*
 * ================================================================
 * JSON helpers
 * ================================================================
 */

static void json_print_string(
    const char *value)
{
    putchar('"');


    if (value != NULL)
    {
        while (*value != '\0')
        {
            unsigned char c =
                (unsigned char)*value;


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
                    /*
                     * JSON control characters must be escaped.
                     */
                    if (c < 0x20)
                    {
                        printf(
                            "\\u%04x",
                            (unsigned int)c
                        );
                    }
                    else
                    {
                        putchar(c);
                    }

                    break;
            }


            value++;
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
        printf(
            "%d",
            value
        );
    }
}


static void json_print_bool(
    bool value)
{
    printf(
        "%s",
        value
            ? "true"
            : "false"
    );
}


static void print_common_header(
    const char *type,
    uint64_t uptime_ms)
{
    printf("{");


    printf(
        "\"schema\":%d",
        TELEMETRY_SCHEMA_VERSION
    );


    printf(
        ",\"device_id\":"
    );

    json_print_string(
        DEVICE_ID
    );


    printf(
        ",\"type\":"
    );

    json_print_string(
        type
    );


    printf(
        ",\"uptime_ms\":%llu",
        (unsigned long long)
        uptime_ms
    );
}


/*
 * ================================================================
 * Measurements
 * ================================================================
 */

void telemetry_emit_measurement(
    const telemetry_measurement_t *measurement)
{
    if (measurement == NULL)
    {
        return;
    }


    print_common_header(
        "measurement",
        measurement->uptime_ms
    );


    /*
     * Status.
     */
    printf(
        ",\"status\":"
    );

    json_print_string(
        measurement->status
    );


    /*
     * Wi-Fi.
     */
    printf(
        ",\"ssid\":"
    );

    json_print_string(
        measurement->ssid
    );


    printf(
        ",\"bssid\":"
    );

    json_print_string(
        measurement->bssid
    );


    printf(
        ",\"channel\":"
    );

    json_print_nullable_int(
        measurement->channel
    );

    printf(
    	",\"rssi_dbm\":%d",
    	measurement->rssi_dbm
    ); 
    /*
     * IP / gateway.
     */
    printf(
        ",\"ip_address\":"
    );

    json_print_string(
        measurement->ip_address
    );


    printf(
        ",\"gateway\":"
    );

    json_print_string(
        measurement->gateway
    );


    printf(
        ",\"gateway_rtt_ms\":"
    );

    json_print_nullable_int(
        measurement->gateway_rtt_ms
    );


    printf(
        ",\"gateway_loss_pct\":%.2f",
        measurement->gateway_loss_pct
    );


    /*
     * DNS.
     */
    printf(
        ",\"dns_server\":"
    );

    json_print_string(
        measurement->dns_server
    );


    printf(
        ",\"dns_test_domain\":"
    );

    json_print_string(
        measurement->dns_test_domain
    );


    printf(
        ",\"dns_latency_ms\":"
    );

    json_print_nullable_int(
        measurement->dns_latency_ms
    );


    /*
     * Internet RTT.
     */
    printf(
        ",\"internet_rtt_ms\":"
    );

    json_print_nullable_int(
        measurement->internet_rtt_ms
    );


    printf(
        ",\"min_rtt_ms\":"
    );

    json_print_nullable_int(
        measurement->min_rtt_ms
    );


    printf(
        ",\"avg_rtt_ms\":%.2f",
        measurement->avg_rtt_ms
    );


    printf(
        ",\"max_rtt_ms\":"
    );

    json_print_nullable_int(
        measurement->max_rtt_ms
    );


    /*
     * Jitter.
     */
    printf(
        ",\"jitter_ms\":"
    );

    json_print_nullable_int(
        measurement->jitter_ms
    );


    printf(
        ",\"avg_jitter_ms\":%.2f",
        measurement->avg_jitter_ms
    );


    /*
     * Packet loss.
     */
    printf(
        ",\"packet_loss_pct\":%.2f",
        measurement->packet_loss_pct
    );


    /*
     * Counters.
     */
    printf(
        ",\"samples\":%u",
        measurement->samples
    );


    printf(
        ",\"successful\":%u",
        measurement->successful
    );


    printf(
        ",\"failed\":%u",
        measurement->failed
    );


    printf(
        ",\"disconnects\":%u",
        measurement->disconnects
    );


    printf(
        ",\"reconnects\":%u",
        measurement->reconnects
    );


    printf(
        ",\"reconnect_attempts\":%u",
        measurement->reconnect_attempts
    );


    printf(
        ",\"bssid_changes\":%u",
        measurement->bssid_changes
    );


    printf(
        ",\"channel_changes\":%u",
        measurement->channel_changes
    );


    printf(
        ",\"gateway_changes\":%u",
        measurement->gateway_changes
    );


    printf(
        ",\"weak_signal_events\":%u",
        measurement->weak_signal_events
    );


    printf(
        ",\"high_latency_events\":%u",
        measurement->high_latency_events
    );


    /*
     * Active states.
     */
    printf(
        ",\"weak_signal_active\":"
    );

    json_print_bool(
        measurement->weak_signal_active
    );


    printf(
        ",\"high_latency_active\":"
    );

    json_print_bool(
        measurement->high_latency_active
    );


    printf("}\n");


    fflush(stdout);
}


/*
 * ================================================================
 * Events
 * ================================================================
 */

void telemetry_emit_event_simple(
    uint64_t uptime_ms,
    const char *event,
    const char *severity)
{
    print_common_header(
        "event",
        uptime_ms
    );


    printf(
        ",\"event\":"
    );

    json_print_string(
        event
    );


    printf(
        ",\"severity\":"
    );

    json_print_string(
        severity
    );


    printf(
        ",\"details\":{}"
    );


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


    printf(
        ",\"event\":"
    );

    json_print_string(
        event
    );


    printf(
        ",\"severity\":"
    );

    json_print_string(
        severity
    );


    printf(
        ",\"details\":{\"old\":"
    );

    json_print_string(
        old_value
    );


    printf(
        ",\"new\":"
    );

    json_print_string(
        new_value
    );


    printf("}}");


    printf("\n");


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


    printf(
        ",\"event\":"
    );

    json_print_string(
        event
    );


    printf(
        ",\"severity\":"
    );

    json_print_string(
        severity
    );


    printf(
        ",\"details\":{"
        "\"old\":%d,"
        "\"new\":%d"
        "}",
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


    printf(
        ",\"event\":"
    );

    json_print_string(
        event
    );


    printf(
        ",\"severity\":"
    );

    json_print_string(
        severity
    );


    printf(
        ",\"details\":{\"metric\":"
    );

    json_print_string(
        metric
    );


    printf(
        ",\"value\":%d"
        ",\"threshold\":%d"
        "}",
        value,
        threshold
    );


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


    printf(
        ",\"event\":"
    );

    json_print_string(
        event
    );


    printf(
        ",\"severity\":"
    );

    json_print_string(
        severity
    );


    printf(
        ",\"details\":{\"duration_ms\":%llu}",
        (unsigned long long)
        duration_ms
    );


    printf("}\n");


    fflush(stdout);
}
