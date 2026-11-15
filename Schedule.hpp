#pragma once

#include "StationConfig.hpp"
#include <cstdint>
#include <ctime>

/**
 * When the station takes readings: every intervalMinutes between wakeHour
 * and sleepHour (local time), and asleep through the night.
 */
struct Schedule {
    uint8_t wakeHour = config::WAKE_HOUR;
    uint8_t sleepHour = config::SLEEP_HOUR;
    uint16_t intervalMinutes = config::INTERVAL_MINUTES;

    static constexpr uint32_t SECONDS_PER_DAY = 24 * 3600;

    /** @brief Whether the hours make sense: 0 <= wakeHour < sleepHour <= 24. */
    constexpr bool valid(void) const {
        return wakeHour < sleepHour && sleepHour <= 24 && intervalMinutes > 0;
    }

    /** @brief Whether local time is within operating hours [wakeHour, sleepHour). */
    bool isOperating(const tm& local) const {
        return local.tm_hour >= wakeHour && local.tm_hour < sleepHour;
    }

    /**
     * @brief Seconds from local time until the station should next wake.
     * During operating hours that's one interval; otherwise it's until wakeHour.
     */
    uint32_t secondsUntilNextWake(const tm& local) const {
        const uint32_t interval = intervalMinutes * 60u;
        if (!valid()) return interval;
        if (isOperating(local)) return interval;

        const uint32_t now = local.tm_hour * 3600u + local.tm_min * 60u + local.tm_sec;
        const uint32_t wake = wakeHour * 3600u;
        return now < wake ? wake - now : SECONDS_PER_DAY - now + wake;
    }
};

static_assert(Schedule{}.valid(), "config::WAKE_HOUR must be before config::SLEEP_HOUR, and the interval non-zero");
