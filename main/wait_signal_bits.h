#ifndef WAIT_SIGNAL_BITS_H
#define WAIT_SIGNAL_BITS_H



/*
 * System-wide synchronization bits
 * Each bit represents a specific system-level event or status.
 * These bits are shared across all components.
 */

// Wi-Fi related events
#define SYNC_BIT_WIFI_INIT_DONE           (1 << 0)
#define SYNC_BIT_WIFI_STA_STARTED         (1 << 1)
#define SYNC_EVENT_DISCOVERY_COMPLETE     (1 << 2)
// Add more as needed (up to 24–32 bits recommended per group)




#endif