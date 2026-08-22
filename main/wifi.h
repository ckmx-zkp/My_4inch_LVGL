#pragma once
#include <stdbool.h>

// Connect to the AP configured via menuconfig. Blocks until connected or
// the retry limit is hit. Netif/event loop must be created by caller? No —
// this function sets up netif + default event loop itself.
void wifi_init_sta(void);

// STA has an IP. Safe to call before wifi_init_sta (returns false).
bool wifi_sta_connected(void);

// Current AP RSSI in dBm (typically -30..-90). 0 if not connected.
int wifi_sta_rssi(void);
