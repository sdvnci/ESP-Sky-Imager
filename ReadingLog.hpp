#pragma once

#include "io.hpp"
#include "SensorContainer.hpp"
#include <functional>
#include <string>

/**
 * Readings that couldn't be delivered, kept on the station's file system
 * until the server takes them.
 *
 * Stored as JSON Lines (one reading per line), so appending never reads the
 * whole log into RAM, and a corrupt line costs only that line.
 */
class ReadingLog {
    public:

        /** What happened when a logged reading was sent. */
        enum class Delivery {
            /** The server stored it - drop it from the log. */
            SENT,
            /** The server will never accept it (eg: invalid) - drop it from the log. */
            REJECTED,
            /** Couldn't deliver it right now - keep it, and stop for this wake. */
            FAILED,
        };

        /** Outcome of a replay. */
        struct ReplayResult {
            size_t sent = 0;
            size_t rejected = 0;
            size_t kept = 0;
        };

        explicit ReadingLog(fs::FS& fs, std::string path = LOG_FILE)
            : _fs(fs), _path(std::move(path)) {}

        /**
         * @brief Add a reading to the end of the log.
         * @return true if it was written.
         */
        [[nodiscard]]
        bool append(const EnvironmentalReading& reading);

        /**
         * @brief Send logged readings, oldest first, and keep only those not delivered.
         * Stops at the first FAILED delivery (the server is likely down again)
         * or after maxSends readings, keeping the rest for the next wake.
         * Nothing is removed from the log unless the replacement log was written.
         * @param send Sends one reading and reports what happened.
         * @param maxSends Most readings to send in this call.
         * @return Counts of readings sent, rejected and kept.
         */
        ReplayResult replay(
            const std::function<Delivery(const EnvironmentalReading&)>& send,
            size_t maxSends
        );

        /** @brief Whether the log has any readings in it. */
        bool empty(void) const;

    private:
        /**
         * Finish a replay interrupted by a reset. If the log still exists the
         * swap never started, so the leftover replacement is dropped; if only
         * the replacement exists, the log was removed but not yet replaced.
         */
        void recover(void);

        std::string pendingPath(void) const { return _path + ".tmp"; }

        fs::FS& _fs;
        const std::string _path;
};
