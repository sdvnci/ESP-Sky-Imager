#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Station-wide settings.
 */
namespace config {
    /**
     * POSIX TZ string for the station's local time, used only for the
     * day/night schedule. Timestamps sent to the server stay in UTC.
     * Saint Lucia is UTC-4 with no daylight saving.
     */
    constexpr const char* TIMEZONE = "AST4";

    /** Local hour to start taking readings. */
    constexpr uint8_t WAKE_HOUR = 6;
    /** Local hour to stop and sleep until WAKE_HOUR. */
    constexpr uint8_t SLEEP_HOUR = 21;
    /** Minutes between readings during the day. */
    constexpr uint16_t INTERVAL_MINUTES = 20;

    /** Re-fetch QNH from the server when the cached value is older than this. */
    constexpr uint32_t QNH_MAX_AGE_S = 2 * 3600;
    /** Re-sync the clock over NTP when the last sync is older than this. */
    constexpr uint32_t NTP_MAX_AGE_S = 6 * 3600;

    /** Most logged readings to upload in one wake, to bound time awake. */
    constexpr size_t MAX_REPLAY_PER_WAKE = 50;

    /** How long to wait for a USB serial monitor at boot (debug builds only). */
    constexpr uint32_t SERIAL_WAIT_MS = 2000;
};
