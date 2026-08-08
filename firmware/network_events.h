#ifndef NETWORK_EVENTS_H
#define NETWORK_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"


#define EVENT_BSSID_LEN 32
#define EVENT_GATEWAY_LEN 32


typedef struct
{
    /*
     * ============================================================
     * Access point state
     * ============================================================
     */

    bool ap_initialized;

    char current_bssid[EVENT_BSSID_LEN];
    char previous_bssid[EVENT_BSSID_LEN];

    int current_channel;
    int previous_channel;


    /*
     * BSSID changes.
     */
    bool bssid_changed;

    uint32_t bssid_change_count;
    uint64_t last_bssid_change_ms;


    /*
     * Channel changes.
     */
    bool channel_changed;

    uint32_t channel_change_count;
    uint64_t last_channel_change_ms;


    /*
     * ============================================================
     * Gateway state
     * ============================================================
     */

    bool gateway_initialized;

    char current_gateway[EVENT_GATEWAY_LEN];
    char previous_gateway[EVENT_GATEWAY_LEN];

    bool gateway_changed;

    uint32_t gateway_change_count;
    uint64_t last_gateway_change_ms;


    /*
     * ============================================================
     * Weak signal
     * ============================================================
     */

    bool weak_signal_active;

    bool weak_signal_event;

    uint32_t weak_signal_event_count;

    uint64_t last_weak_signal_ms;


    /*
     * ============================================================
     * High latency
     * ============================================================
     */

    bool high_latency_active;

    bool high_latency_event;

    uint32_t high_latency_event_count;

    uint64_t last_high_latency_ms;


    /*
     * ============================================================
     * High jitter
     * ============================================================
     */

    bool high_jitter_active;

    bool high_jitter_event;

    uint32_t high_jitter_event_count;

    uint64_t last_high_jitter_ms;


    /*
     * ============================================================
     * Rolling packet-loss monitoring
     * ============================================================
     */

    uint8_t packet_loss_window[
        PACKET_LOSS_WINDOW_SIZE
    ];

    uint32_t packet_loss_window_index;

    uint32_t packet_loss_window_count;

    uint32_t packet_loss_window_failures;

    float current_packet_loss_window_pct;


    bool high_packet_loss_active;

    bool high_packet_loss_event;

    uint32_t high_packet_loss_event_count;

    uint64_t last_high_packet_loss_ms;


    /*
     * ============================================================
     * Internet outage monitoring
     * ============================================================
     */

    uint32_t internet_failure_streak;


    bool internet_outage_active;

    bool internet_outage_event;

    bool internet_recovered_event;


    uint32_t internet_outage_count;


    uint64_t internet_outage_start_ms;

    uint64_t last_internet_outage_ms;

    uint64_t last_internet_recovery_ms;

    uint64_t last_internet_outage_duration_ms;


    /*
     * ============================================================
     * DNS monitoring
     * ============================================================
     */

    uint32_t dns_failure_streak;


    bool dns_failure_active;

    bool dns_failure_event;

    bool dns_recovered_event;


    uint32_t dns_failure_count;


    uint64_t last_dns_failure_ms;

    uint64_t last_dns_recovery_ms;

} network_events_t;


/*
 * Initialize event subsystem.
 */
void network_events_init(
    network_events_t *events
);


/*
 * Clear event flags that should only be true
 * for one monitoring cycle.
 */
void network_events_begin_sample(
    network_events_t *events
);


/*
 * BSSID / channel tracking.
 */
void network_events_update_ap(
    network_events_t *events,
    const char *bssid,
    int channel,
    uint64_t now_ms
);


/*
 * Gateway tracking.
 */
void network_events_update_gateway(
    network_events_t *events,
    const char *gateway,
    uint64_t now_ms
);


/*
 * Weak RSSI monitoring.
 */
void network_events_update_signal(
    network_events_t *events,
    int rssi,
    int weak_threshold_dbm,
    uint64_t now_ms
);


/*
 * High RTT monitoring.
 */
void network_events_update_latency(
    network_events_t *events,
    int rtt_ms,
    int threshold_ms,
    uint64_t now_ms
);


/*
 * High jitter monitoring.
 */
void network_events_update_jitter(
    network_events_t *events,
    int jitter_ms,
    int threshold_ms,
    uint64_t now_ms
);


/*
 * Rolling packet-loss monitoring.
 *
 * probe_success:
 *
 * true  = Internet probe succeeded
 * false = Internet probe failed
 */
void network_events_update_packet_loss(
    network_events_t *events,
    bool probe_success,
    float threshold_pct,
    uint32_t minimum_samples,
    uint64_t now_ms
);


/*
 * Internet outage monitoring.
 *
 * eligible means:
 *
 * Wi-Fi is connected AND gateway is reachable.
 *
 * This prevents a Wi-Fi failure from also being
 * incorrectly labelled as an Internet outage.
 */
void network_events_update_internet(
    network_events_t *events,
    bool eligible,
    bool internet_success,
    uint32_t failure_threshold,
    uint64_t now_ms
);


/*
 * DNS failure/recovery monitoring.
 *
 * Only call this when IP Internet connectivity
 * itself is working.
 */
void network_events_update_dns(
    network_events_t *events,
    bool dns_success,
    uint32_t failure_threshold,
    uint64_t now_ms
);


#endif
