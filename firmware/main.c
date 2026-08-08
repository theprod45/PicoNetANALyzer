#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "config.h"

#define WIZ_UART uart1
#define WIZ_TX_PIN 4
#define WIZ_RX_PIN 5

#define RESPONSE_SIZE 1024


typedef struct
{
    char ssid[64];
    char bssid[32];

    int channel;
    int rssi;

    bool valid;

} wifi_info_t;


/*
 * Send an AT command to the WizFi360.
 *
 * The response is captured in a buffer.
 * Raw AT output is not printed to USB.
 */
static bool wizfi_command(
    const char *command,
    char *response,
    size_t response_size,
    uint32_t timeout_ms)
{
    /*
     * Remove any old bytes waiting in UART.
     */
    while (uart_is_readable(WIZ_UART))
    {
        uart_getc(WIZ_UART);
    }

    response[0] = '\0';

    /*
     * Send command.
     */
    uart_puts(WIZ_UART, command);
    uart_puts(WIZ_UART, "\r\n");

    absolute_time_t timeout =
        make_timeout_time_ms(timeout_ms);

    size_t pos = 0;

    while (!time_reached(timeout))
    {
        while (uart_is_readable(WIZ_UART))
        {
            char c = uart_getc(WIZ_UART);

            if (pos < response_size - 1)
            {
                response[pos++] = c;
                response[pos] = '\0';
            }

            /*
             * Successful AT command.
             */
            if (strstr(response, "\r\nOK\r\n") != NULL ||
                strstr(response, "\nOK\r\n") != NULL)
            {
                return true;
            }

            /*
             * Failed AT command.
             */
            if (strstr(response, "ERROR") != NULL ||
                strstr(response, "FAIL") != NULL)
            {
                return false;
            }
        }

        tight_loop_contents();
    }

    return false;
}


/*
 * Connect to configured Wi-Fi network.
 */
static bool connect_wifi(
    char *response,
    size_t response_size)
{
    /*
     * Station mode.
     */
    wizfi_command(
        "AT+CWMODE_CUR=1",
        response,
        response_size,
        2000
    );

    char command[128];

    snprintf(
        command,
        sizeof(command),
        "AT+CWJAP_CUR=\"%s\",\"%s\"",
        WIFI_SSID,
        WIFI_PASSWORD
    );

    return wizfi_command(
        command,
        response,
        response_size,
        15000
    );
}


/*
 * Get information about the currently
 * associated Wi-Fi access point.
 */
static wifi_info_t get_wifi_info(void)
{
    wifi_info_t info = {0};

    char response[RESPONSE_SIZE];

    if (!wizfi_command(
            "AT+CWJAP_CUR?",
            response,
            sizeof(response),
            2000))
    {
        return info;
    }

    char *line =
        strstr(response, "+CWJAP_CUR:");

    if (line == NULL)
    {
        return info;
    }

    /*
     * Expected:
     *
     * +CWJAP_CUR:"SSID","BSSID",channel,RSSI
     */
    int result = sscanf(
        line,
        "+CWJAP_CUR:\"%63[^\"]\",\"%31[^\"]\",%d,%d",
        info.ssid,
        info.bssid,
        &info.channel,
        &info.rssi
    );

    if (result == 4)
    {
        info.valid = true;
    }

    return info;
}


/*
 * Read the default gateway IP.
 */
static bool get_gateway(
    char *gateway,
    size_t size)
{
    char response[RESPONSE_SIZE];

    if (!wizfi_command(
            "AT+CIPSTA_CUR?",
            response,
            sizeof(response),
            2000))
    {
        return false;
    }

    const char *prefix =
        "+CIPSTA_CUR:gateway:\"";

    char *start =
        strstr(response, prefix);

    if (start == NULL)
    {
        return false;
    }

    start += strlen(prefix);

    char *end =
        strchr(start, '"');

    if (end == NULL)
    {
        return false;
    }

    size_t length =
        (size_t)(end - start);

    if (length >= size)
    {
        length = size - 1;
    }

    memcpy(
        gateway,
        start,
        length
    );

    gateway[length] = '\0';

    return true;
}


/*
 * Ping a host.
 *
 * Returns:
 *
 * >= 0    RTT in milliseconds
 * -1      failure / timeout
 */
static int ping_host(const char *host)
{
    char response[RESPONSE_SIZE];
    char command[128];

    snprintf(
        command,
        sizeof(command),
        "AT+PING=\"%s\"",
        host
    );

    if (!wizfi_command(
            command,
            response,
            sizeof(response),
            5000))
    {
        return -1;
    }

    /*
     * Successful response:
     *
     * +23
     *
     * OK
     */
    char *plus =
        strchr(response, '+');

    if (plus == NULL)
    {
        return -1;
    }

    /*
     * Reject responses such as +timeout.
     */
    if (!isdigit((unsigned char)plus[1]))
    {
        return -1;
    }

    return atoi(plus + 1);
}


/*
 * Print a millisecond value as HH:MM:SS.
 */
static void print_hms(uint64_t milliseconds)
{
    uint64_t seconds =
        milliseconds / 1000ULL;

    uint64_t hours =
        seconds / 3600ULL;

    uint64_t minutes =
        (seconds % 3600ULL) / 60ULL;

    uint64_t secs =
        seconds % 60ULL;

    printf(
        "%02llu:%02llu:%02llu",
        (unsigned long long)hours,
        (unsigned long long)minutes,
        (unsigned long long)secs
    );
}


/*
 * Print seconds with two decimal places
 * without depending on the system clock.
 */
static void print_seconds(uint64_t milliseconds)
{
    uint64_t whole =
        milliseconds / 1000ULL;

    uint64_t fraction =
        (milliseconds % 1000ULL) / 10ULL;

    printf(
        "%llu.%02llu s",
        (unsigned long long)whole,
        (unsigned long long)fraction
    );
}


int main(void)
{
    /*
     * USB serial.
     */
    stdio_init_all();

    sleep_ms(3000);

    /*
     * UART between RP2040 and WizFi360.
     */
    uart_init(
        WIZ_UART,
        115200
    );

    gpio_set_function(
        WIZ_TX_PIN,
        GPIO_FUNC_UART
    );

    gpio_set_function(
        WIZ_RX_PIN,
        GPIO_FUNC_UART
    );

    sleep_ms(1000);

    char response[RESPONSE_SIZE];


    /*
     * Verify WizFi360 is responding.
     */
    if (!wizfi_command(
            "AT",
            response,
            sizeof(response),
            1000))
    {
        printf("PicoNetANALyzer\n");
        printf("----------------\n");
        printf("Status: WizFi360 not responding\n");

        while (true)
        {
            sleep_ms(1000);
        }
    }


    /*
     * Disable AT command echo.
     */
    wizfi_command(
        "ATE0",
        response,
        sizeof(response),
        1000
    );


    /*
     * Initial Wi-Fi connection.
     *
     * Continue retrying instead of permanently
     * stopping if ASK4 is temporarily unavailable.
     */
    while (!connect_wifi(
        response,
        sizeof(response)))
    {
        printf("\033[2J\033[H");

        printf("PicoNetANALyzer\n");
        printf("----------------\n");
        printf("Status:       CONNECTING\n");
        printf("Network:      %s\n", WIFI_SSID);
        printf("\nRetrying...\n");

        fflush(stdout);

        sleep_ms(5000);
    }


    /*
     * Get gateway after initial connection.
     */
    char gateway[32] = "Unknown";

    if (!get_gateway(
            gateway,
            sizeof(gateway)))
    {
        snprintf(
            gateway,
            sizeof(gateway),
            "Unknown"
        );
    }


    /*
     * General probe statistics.
     */
    uint32_t samples = 0;

    uint32_t internet_failures = 0;
    uint32_t gateway_failures = 0;


    /*
     * RTT statistics.
     */
    int min_rtt = -1;
    int max_rtt = -1;

    uint64_t total_rtt = 0;
    uint32_t successful_rtt_samples = 0;


    /*
     * Jitter statistics.
     */
    int previous_internet_ping = -1;
    int current_jitter = -1;

    uint64_t total_jitter = 0;
    uint32_t jitter_samples = 0;


    /*
     * Disconnect/reconnect statistics.
     */
    bool previous_wifi_connected = true;
    bool outage_active = false;

    uint32_t disconnect_count = 0;
    uint32_t reconnect_count = 0;
    uint32_t reconnect_attempts = 0;

    uint64_t outage_start_ms = 0;

    uint64_t last_disconnect_at_ms = 0;

    uint64_t last_reconnect_duration_ms = 0;

    uint64_t total_outage_ms = 0;


    while (true)
    {
        samples++;

        uint64_t now_ms =
            time_us_64() / 1000ULL;


        /*
         * Check whether we're still associated
         * with the Wi-Fi access point.
         */
        wifi_info_t wifi =
            get_wifi_info();

        bool wifi_connected =
            wifi.valid;


        /*
         * Detect connected -> disconnected edge.
         */
        if (!wifi_connected &&
            previous_wifi_connected)
        {
            disconnect_count++;

            outage_active = true;

            outage_start_ms =
                now_ms;

            last_disconnect_at_ms =
                now_ms;
        }


        /*
         * If disconnected, attempt to reconnect.
         */
        if (!wifi_connected)
        {
            reconnect_attempts++;

            sleep_ms(RECONNECT_DELAY_MS);

            bool reconnect_result =
                connect_wifi(
                    response,
                    sizeof(response)
                );

            if (reconnect_result)
            {
                /*
                 * Verify association instead of
                 * trusting only the join command.
                 */
                wifi =
                    get_wifi_info();

                wifi_connected =
                    wifi.valid;
            }
        }


        /*
         * Detect successful recovery.
         *
         * This also handles the case where the
         * WizFi360 reconnects by itself.
         */
        if (wifi_connected &&
            outage_active)
        {
            uint64_t recovered_at_ms =
                time_us_64() / 1000ULL;

            last_reconnect_duration_ms =
                recovered_at_ms -
                outage_start_ms;

            total_outage_ms +=
                last_reconnect_duration_ms;

            reconnect_count++;

            outage_active = false;

            /*
             * DHCP information may have changed,
             * so refresh the gateway.
             */
            snprintf(
                gateway,
                sizeof(gateway),
                "Unknown"
            );

            get_gateway(
                gateway,
                sizeof(gateway)
            );
        }


        /*
         * Measure network only while associated.
         */
        int gateway_ping = -1;
        int internet_ping = -1;

        if (wifi_connected)
        {
            if (strcmp(
                    gateway,
                    "Unknown") != 0)
            {
                gateway_ping =
                    ping_host(gateway);
            }

            internet_ping =
                ping_host(INTERNET_TARGET);
        }


        /*
         * Count failures.
         *
         * A Wi-Fi outage counts as an Internet
         * reachability failure too.
         */
        if (gateway_ping < 0)
        {
            gateway_failures++;
        }

        if (internet_ping < 0)
        {
            internet_failures++;
        }


        /*
         * RTT statistics.
         *
         * Failed probes are NOT included
         * in RTT averages.
         */
        if (internet_ping >= 0)
        {
            if (min_rtt < 0 ||
                internet_ping < min_rtt)
            {
                min_rtt =
                    internet_ping;
            }

            if (max_rtt < 0 ||
                internet_ping > max_rtt)
            {
                max_rtt =
                    internet_ping;
            }

            total_rtt +=
                (uint64_t)internet_ping;

            successful_rtt_samples++;
        }


        /*
         * Average RTT.
         */
        float average_rtt = 0.0f;

        if (successful_rtt_samples > 0)
        {
            average_rtt =
                (float)total_rtt /
                (float)successful_rtt_samples;
        }


        /*
         * Current jitter.
         */
        if (internet_ping >= 0)
        {
            if (previous_internet_ping >= 0)
            {
                current_jitter =
                    abs(
                        internet_ping -
                        previous_internet_ping
                    );

                total_jitter +=
                    (uint64_t)current_jitter;

                jitter_samples++;
            }
            else
            {
                current_jitter = -1;
            }

            previous_internet_ping =
                internet_ping;
        }
        else
        {
            /*
             * Break the consecutive RTT chain
             * during an outage.
             */
            current_jitter = -1;
            previous_internet_ping = -1;
        }


        /*
         * Average jitter.
         */
        float average_jitter = 0.0f;

        if (jitter_samples > 0)
        {
            average_jitter =
                (float)total_jitter /
                (float)jitter_samples;
        }


        /*
         * Internet packet loss.
         */
        float internet_loss =
            ((float)internet_failures /
             (float)samples) * 100.0f;


        /*
         * Gateway failure percentage.
         */
        float gateway_loss =
            ((float)gateway_failures /
             (float)samples) * 100.0f;


        uint64_t display_now_ms =
            time_us_64() / 1000ULL;


        uint64_t current_outage_ms = 0;

        if (outage_active)
        {
            current_outage_ms =
                display_now_ms -
                outage_start_ms;
        }


        /*
         * Clear terminal.
         */
        printf("\033[2J\033[H");


        /*
         * Title.
         */
        printf("PicoNetANALyzer\n");
        printf("----------------\n");


        /*
         * Overall status.
         */
        if (!wifi_connected)
        {
            printf(
                "Status:       DISCONNECTED / RECONNECTING\n"
            );
        }
        else if (gateway_ping < 0)
        {
            printf(
                "Status:       LOCAL NETWORK ISSUE\n"
            );
        }
        else if (internet_ping < 0)
        {
            printf(
                "Status:       INTERNET ISSUE\n"
            );
        }
        else
        {
            printf(
                "Status:       ONLINE\n"
            );
        }


        /*
         * Wi-Fi information.
         */
        printf("\n");

        if (wifi_connected &&
            wifi.valid)
        {
            printf(
                "SSID:         %s\n",
                wifi.ssid
            );

            printf(
                "BSSID:        %s\n",
                wifi.bssid
            );

            printf(
                "Channel:      %d\n",
                wifi.channel
            );

            printf(
                "RSSI:         %d dBm\n",
                wifi.rssi
            );
        }
        else
        {
            printf("SSID:         --\n");
            printf("BSSID:        --\n");
            printf("Channel:      --\n");
            printf("RSSI:         --\n");
        }


        /*
         * Gateway measurements.
         */
        printf("\n");

        printf(
            "Gateway:      %s\n",
            gateway
        );

        if (gateway_ping >= 0)
        {
            printf(
                "Gateway RTT:  %d ms\n",
                gateway_ping
            );
        }
        else
        {
            printf(
                "Gateway RTT:  TIMEOUT\n"
            );
        }

        printf(
            "Gateway Loss: %.2f%%\n",
            gateway_loss
        );


        /*
         * Internet measurements.
         */
        printf("\n");

        if (internet_ping >= 0)
        {
            printf(
                "Internet RTT: %d ms\n",
                internet_ping
            );
        }
        else
        {
            printf(
                "Internet RTT: TIMEOUT\n"
            );
        }


        /*
         * RTT history.
         */
        if (successful_rtt_samples > 0)
        {
            printf(
                "Min RTT:      %d ms\n",
                min_rtt
            );

            printf(
                "Avg RTT:      %.2f ms\n",
                average_rtt
            );

            printf(
                "Max RTT:      %d ms\n",
                max_rtt
            );
        }
        else
        {
            printf("Min RTT:      --\n");
            printf("Avg RTT:      --\n");
            printf("Max RTT:      --\n");
        }


        /*
         * Jitter.
         */
        printf("\n");

        if (current_jitter >= 0)
        {
            printf(
                "Jitter:       %d ms\n",
                current_jitter
            );
        }
        else
        {
            printf(
                "Jitter:       --\n"
            );
        }

        if (jitter_samples > 0)
        {
            printf(
                "Avg Jitter:   %.2f ms\n",
                average_jitter
            );
        }
        else
        {
            printf(
                "Avg Jitter:   --\n"
            );
        }

        printf(
            "Packet Loss:  %.2f%%\n",
            internet_loss
        );


        /*
         * Connection event statistics.
         */
        printf("\n");
        printf("Connection Events\n");
        printf("-----------------\n");

        printf(
            "Disconnects:  %lu\n",
            (unsigned long)disconnect_count
        );

        printf(
            "Reconnects:   %lu\n",
            (unsigned long)reconnect_count
        );

        printf(
            "Reconnect Try:%lu\n",
            (unsigned long)reconnect_attempts
        );


        if (disconnect_count > 0)
        {
            printf(
                "Last Drop:    "
            );

            print_hms(
                last_disconnect_at_ms
            );

            printf(
                " uptime\n"
            );
        }
        else
        {
            printf(
                "Last Drop:    --\n"
            );
        }


        if (reconnect_count > 0)
        {
            printf(
                "Last Recovery:"
            );

            print_seconds(
                last_reconnect_duration_ms
            );

            printf("\n");
        }
        else
        {
            printf(
                "Last Recovery:--\n"
            );
        }


        printf(
            "Total Outage: "
        );

        print_seconds(
            total_outage_ms
        );

        printf("\n");


        if (outage_active)
        {
            printf(
                "Current Outage:"
            );

            print_seconds(
                current_outage_ms
            );

            printf("\n");
        }
        else
        {
            printf(
                "Current Outage:--\n"
            );
        }


        /*
         * Runtime information.
         */
        printf("\n");

        printf(
            "Samples:      %lu\n",
            (unsigned long)samples
        );

        printf(
            "Successful:   %lu\n",
            (unsigned long)
                successful_rtt_samples
        );

        printf(
            "Failed:       %lu\n",
            (unsigned long)
                internet_failures
        );

        printf(
            "Uptime:       "
        );

        print_hms(
            display_now_ms
        );

        printf("\n");

        fflush(stdout);


        /*
         * Remember state for edge detection.
         */
        previous_wifi_connected =
            wifi_connected;


        sleep_ms(
            MONITOR_INTERVAL_MS
        );
    }
}
