#include <AUnit.h>
#include "Schedule.hpp"

namespace {
    tm at(int hour, int minute = 0, int second = 0) {
        tm t{};
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = second;
        return t;
    }
}


test(Schedule_DaytimeIsOneInterval) {
    const Schedule s;
    assertTrue(s.isOperating(at(12)));
    assertEqual(s.secondsUntilNextWake(at(12)), (uint32_t)config::INTERVAL_MINUTES * 60);
    assertEqual(s.secondsUntilNextWake(at(20, 59, 59)), (uint32_t)config::INTERVAL_MINUTES * 60);
}

test(Schedule_NightSleepsUntilWakeHour) {
    const Schedule s{6, 21, 20};
    assertFalse(s.isOperating(at(21)));
    assertEqual(s.secondsUntilNextWake(at(21)), (uint32_t)9 * 3600);
    assertEqual(s.secondsUntilNextWake(at(23, 30, 15)), (uint32_t)(86400 - (23 * 3600 + 30 * 60 + 15) + 6 * 3600));
    assertEqual(s.secondsUntilNextWake(at(0)), (uint32_t)6 * 3600);
    assertEqual(s.secondsUntilNextWake(at(5, 59, 59)), (uint32_t)1);
    assertTrue(s.isOperating(at(6)));
}

test(Schedule_InvalidFallsBackToInterval) {
    const Schedule s{21, 6, 15};
    assertFalse(s.valid());
    assertEqual(s.secondsUntilNextWake(at(3)), (uint32_t)15 * 60);
}
