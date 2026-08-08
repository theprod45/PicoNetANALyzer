# PicoNetANALyzer

PicoNetANALyzer is an embedded Wi-Fi network monitoring and diagnostics device built using the WIZnet WizFi360-EVB-Pico.

The project uses the RP2040 to communicate with the onboard WizFi360 Wi-Fi module over UART and continuously monitor network health.

## Current Features

- Wi-Fi connection monitoring
- SSID detection
- BSSID detection
- Wi-Fi channel monitoring
- RSSI measurement
- Default gateway detection
- Gateway RTT measurement
- Internet RTT measurement
- Minimum RTT
- Average RTT
- Maximum RTT
- Jitter measurement
- Average jitter
- Packet-loss measurement
- Wi-Fi disconnect detection
- Automatic reconnection
- Reconnect timing
- Network outage tracking
- Device uptime

## Hardware

- WIZnet WizFi360-EVB-Pico
- RP2040
- WizFi360 Wi-Fi module
- USB connection for serial monitoring

## Architecture

```text
Internet
   |
Wi-Fi Network
   |
WizFi360
   |
 UART
   |
RP2040
   |
 USB
   |
Host Computer
