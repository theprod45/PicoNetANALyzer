#ifndef NETWORK_EVENTS_H
#define NETWORK_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

#define EVENT_BSSID_LEN 32
#define EVENT_GATEWAY_LEN 32


typedef struct
{
    /*
     * Access-point state.
     */
    bool ap_initialized;

    char current_bssid[EVENT_BSSID_LEN];
    char previous_bssid[EVENT_BSSID_LEN];

    int current_channel;
    int previous_channel;


    /*
     * BSSID-change tracking.
     */
    bool bssid_changed;

    uint32_t bssid_change_count;
    uint64_t last_bssid_change_ms;


    /*
     * Channel-change tracking.
     */
    bool channel_changed;

    uint32_t channel_change_count;
    uint64_t last_channel_change_ms;


    /*
     * Gateway tracking.
     */
    bool gateway_initialized;

    char current_gateway[EVENT_GATEWAY_LEN];
    char previous_gateway[EVENT_GATEWAY_LEN];

    bool gateway_changed;

    uint32_t gateway_change_count;
    uint64_t last_gateway_change_ms;


    /*
     * Weak-signal tracking.
     */
    bool weak_signal_active;
    bool weak_signal_event;

    uint32_t weak_signal_event_count;
    uint64_t last_weak_signal_ms;


    /*
     * High-latency tracking.
     */
    bool high_latency_active;
    bool high_latency_event;

    uint32_t high_latency_event_count;
    uint64_t last_high_latency_ms;

} network_events_t;


/*
 * Initialize all event state.
 */
void network_events_init(
    network_events_t *events
);


/*
 * Clear event flags that should only remain true
 * for one monitoring cycle.
 */
void network_events_begin_sample(
    network_events_t *events
);


/*
 * Track BSSID and Wi-Fi channel.
 */
void network_events_update_ap(
    network_events_t *events,
    const char *bssid,
    int channel,
    uint64_t now_ms
);


/*
 * Track default-gateway changes.
 */
void network_events_update_gateway(
    network_events_t *events,
    const char *gateway,
    uint64_t now_ms
);


/*
 * Track transitions into weak-signal state.
 */
void network_events_update_signal(
    network_events_t *events,
    int rssi,
    int weak_threshold_dbm,
    uint64_t now_ms
);


/*
 * Track transitions into high-latency state.
 *
 * RTT < 0 means the probe failed.
 */
void network_events_update_latency(
    network_events_t *events,
    int rtt_ms,
    int high_latency_threshold_ms,
    uint64_t now_ms
);


#endif
