#ifndef CONFIG_H
#define CONFIG_H

// Device ID
#define DEVICE_ID "pico-01"

/*
 * PicoNetANALyzer configuration template
 *
 * Copy this file:
 *
 *     cp config.example.h config.h
 *
 * Then edit config.h with your own settings.
 */

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

/*
 * Internet host used for latency/connectivity tests.
 */
#define INTERNET_TARGET "1.1.1.1"

/*
 * Time between network measurements.
 */
#define MONITOR_INTERVAL_MS 5000

/*
 * Delay before attempting Wi-Fi reconnection.
 */
#define RECONNECT_DELAY_MS 2000

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define INTERNET_TARGET "1.1.1.1"

#define MONITOR_INTERVAL_MS 5000
#define RECONNECT_DELAY_MS 2000

#define WEAK_RSSI_THRESHOLD_DBM -75
#define HIGH_RTT_THRESHOLD_MS 150

/*
 * High jitter threshold.
 */
#define HIGH_JITTER_THRESHOLD_MS 50


/*
 * Packet-loss monitoring.
 *
 * Last 12 Internet probes are considered.
 * At a 5-second monitoring interval this is
 * approximately a one-minute window.
 */
#define PACKET_LOSS_WINDOW_SIZE 12

#define PACKET_LOSS_MIN_SAMPLES 5

#define HIGH_PACKET_LOSS_THRESHOLD_PCT 20


/*
 * Require this many consecutive failed Internet
 * probes before declaring an Internet outage.
 */
#define INTERNET_FAILURE_THRESHOLD 2


/*
 * DNS monitoring.
 */
#define DNS_TEST_DOMAIN "example.com"

#define DNS_FAILURE_THRESHOLD 2

#endif
