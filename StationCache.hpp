#pragma once

#include "io.hpp"
#include <ctime>
#include <optional>
#include <string>

/**
 * Values worth keeping between wakes so the station doesn't hit the network
 * every time: the last QNH and when the clock was last synced over NTP.
 *
 * Loaded from the file system on construction and written back on
 * destruction if anything changed. Times are stored as Unix seconds, so
 * there's no timezone or string parsing involved.
 *
 * NOTE: deep sleep never returns, so destructors in its scope don't run -
 * let the cache go out of scope (or call save()) before sleeping.
 */
class StationCache {
    public:
        explicit StationCache(fs::FS& fs, std::string path = CACHE_FILE);
        ~StationCache();

        StationCache(const StationCache&) = delete;
        StationCache& operator=(const StationCache&) = delete;

        /**
         * @brief The cached QNH if it was fetched within maxAgeSeconds of now.
         * @param now Current Unix time.
         */
        std::optional<float> freshQnh(time_t now, uint32_t maxAgeSeconds) const;

        /** @brief The last QNH fetched, however old. */
        std::optional<float> lastQnh(void) const { return _qnh; }

        void setQnh(float qnhHpa, time_t now);

        /**
         * @brief Whether the clock was synced over NTP within maxAgeSeconds of now.
         * @param now Current Unix time.
         */
        bool ntpFresh(time_t now, uint32_t maxAgeSeconds) const;

        void setNtpSynced(time_t now);

        /**
         * @brief Write the cache to the file system now.
         * @return true if it was written (or there was nothing to write).
         */
        [[nodiscard]]
        bool save(void);

    private:
        /** Whether `at` is within maxAgeSeconds before now. A clock that went backwards counts as stale. */
        static bool withinAge(time_t at, time_t now, uint32_t maxAgeSeconds);

        fs::FS& _fs;
        const std::string _path;

        std::optional<float> _qnh;
        time_t _qnh_at = 0;
        time_t _ntp_at = 0;
        bool _dirty = false;
};
