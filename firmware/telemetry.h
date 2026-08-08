#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

#define TELEMETRY_SCHEMA_VERSION 1


typedef struct
{
    uint64_t uptime_ms;

    const char *status;

    const char *ssid;
    const char *bssid;

    int channel;
    int rssi_dbm;

    const char *gateway;

    int gateway_rtt_ms;
    float gateway_loss_pct;

    int internet_rtt_ms;

    int min_rtt_ms;
    float avg_rtt_ms;
    int max_rtt_ms;

    int jitter_ms;
    float avg_jitter_ms;

    float packet_loss_pct;

    uint32_t samples;
    uint32_t successful;
    uint32_t failed;

    uint32_t disconnects;
    uint32_t reconnects;
    uint32_t reconnect_attempts;

    uint32_t bssid_changes;
    uint32_t channel_changes;
    uint32_t gateway_changes;

    uint32_t weak_signal_events;
    uint32_t high_latency_events;

    bool weak_signal_active;
    bool high_latency_active;

} telemetry_measurement_t;


/*
 * Emit one complete measurement JSON object.
 */
void telemetry_emit_measurement(
    const telemetry_measurement_t *measurement
);


/*
 * Generic event with no additional values.
 */
void telemetry_emit_event_simple(
    uint64_t uptime_ms,
    const char *event,
    const char *severity
);


/*
 * Event representing a string value changing.
 */
void telemetry_emit_event_change_string(
    uint64_t uptime_ms,
    const char *event,
    const char *severity,
    const char *old_value,
    const char *new_value
);


/*
 * Event representing an integer value changing.
 */
void telemetry_emit_event_change_int(
    uint64_t uptime_ms,
    const char *event,
    const char *severity,
    int old_value,
    int new_value
);


/*
 * Event representing a metric crossing a threshold.
 */
void telemetry_emit_event_metric(
    uint64_t uptime_ms,
    const char *event,
    const char *severity,
    const char *metric,
    int value,
    int threshold
);


/*
 * Event containing a duration.
 */
void telemetry_emit_event_duration(
    uint64_t uptime_ms,
    const char *event,
    const char *severity,
    uint64_t duration_ms
);

#endif
