#include "io.hpp"
#include "communications.hpp"
#include "SensorContainer.hpp"
#include "StationConfig.hpp"
#include "StationCache.hpp"
#include "ReadingLog.hpp"
#include "Schedule.hpp"
#include <esp_sleep.h>

/**
 * @brief Whether the system clock holds a real time (set by NTP now, or
 * before a deep sleep - the RTC keeps counting through it).
 */
bool clockIsSet(void) {
    return time(nullptr) > 1577836800;  // January 1, 2020
}

/**
 * @brief The current local time, for the schedule.
 */
tm localNow(void) {
    const time_t now = time(nullptr);
    tm local;
    localtime_r(&now, &local);
    return local;
}

/**
 * @brief Map a send outcome onto what the log should do with the reading.
 */
ReadingLog::Delivery delivery(const HttpResponse& response) {
    if (response.ok()) return ReadingLog::Delivery::SENT;
    if (response.rejected()) return ReadingLog::Delivery::REJECTED;
    return ReadingLog::Delivery::FAILED;
}

/**
 * One wake: take a reading and deliver it, or log it for later.
 *
 * Everything the cycle owns (sensors, cache, files, connections) is released
 * when this returns - deep sleep never returns, so nothing in its scope
 * would be cleaned up.
 *
 * @return Seconds to sleep before the next wake.
 */
uint32_t runCycle(void) {
    const Schedule schedule;

    // The RTC keeps time through deep sleep, so at night we can go straight
    // back to sleep without powering up WiFi or the sensors.
    if (clockIsSet() && !schedule.isOperating(localNow())) {
        debugln("Outside operating hours.");
        return schedule.secondsUntilNextWake(localNow());
    }

    fs::FS& fs = determineFileSystem();
    StationCache cache(fs);

    SensorContainer sense;
    if (!sense.begin()) {
        debugln("No environmental sensors found.");
    }

    WifiSpec wifi;
    const bool online = wifi.wifiConnectionSetup(fs, sense.status);

    // Resync the clock if it was never set, or the last sync is getting old.
    if (online && (!clockIsSet() || !cache.ntpFresh(time(nullptr), config::NTP_MAX_AGE_S))) {
        if (wifi.setClock()) {
            cache.setNtpSynced(time(nullptr));
        }
    }

    if (!wifi.getInternalTime(2)) {
        // Without a time the reading can't be placed - there's nothing useful to keep.
        debugln("Clock not set; skipping this reading.");
        return schedule.intervalMinutes * 60u;
    }

    if (!schedule.isOperating(localNow())) {
        debugln("Outside operating hours.");
        return schedule.secondsUntilNextWake(localNow());
    }

    const time_t now = time(nullptr);
    const ServerInfo server = ServerInfo::fromFile(fs);
    const bool serverUp = online && server.websiteReachable(wifi);

    // QNH: fresh cache, else the server, else whatever we last had.
    std::optional<float> qnh = cache.freshQnh(now, config::QNH_MAX_AGE_S);
    if (!qnh && serverUp) {
        qnh = server.fetchQnh(wifi);
        if (qnh) cache.setQnh(*qnh, now);
    }
    if (!qnh) qnh = cache.lastQnh();

    const EnvironmentalReading reading = sense.read(formatTime(wifi.timeinfo), qnh.value_or(NAN));
    debugf("Reading: %s\n", reading.toJson().c_str());

    ReadingLog log(fs);
    bool delivered = false;

    if (serverUp) {
        (void)server.sendStatuses(wifi, sense.status);

        const ReadingLog::Delivery sent = delivery(server.sendReading(wifi, reading));
        delivered = sent != ReadingLog::Delivery::FAILED;

        if (delivered) {
            // The server is taking readings - send the backlog too.
            log.replay(
                [&](const EnvironmentalReading& logged) {
                    return delivery(server.sendReading(wifi, logged));
                },
                config::MAX_REPLAY_PER_WAKE
            );
        }

        (void)server.getFirmwareVersion(wifi);
    }

    if (!delivered && !log.append(reading)) {
        debugln("Failed to log reading - it is lost.");
    }

    return schedule.secondsUntilNextWake(localNow());
}

void setup() {
    #ifdef DEBUG
        Serial.begin(115200);
        // Give a USB serial monitor a moment to attach, but never block a
        // station running on its own.
        const uint32_t start = millis();
        while (!Serial && millis() - start < config::SERIAL_WAIT_MS) {
            delay(10);
        }
    #endif

    debugln("Starting ESP-Sky-Imager...");

    // Timestamps are UTC; the local zone is only for the day/night schedule.
    setenv("TZ", config::TIMEZONE, 1);
    tzset();

    const uint32_t sleepSeconds = runCycle();

    debugf("Sleeping for %u seconds.\n", (unsigned)sleepSeconds);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {
    // Never reached - every wake runs setup() and goes back to deep sleep.
}
