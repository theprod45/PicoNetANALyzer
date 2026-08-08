#ifndef CONFIG_H
#define CONFIG_H

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

#endif
