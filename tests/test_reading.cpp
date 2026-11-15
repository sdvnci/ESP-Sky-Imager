#include <AUnit.h>
#include "SensorContainer.hpp"


test(Reading_JsonRoundTripKeepsNaN) {
    const EnvironmentalReading original("2026-11-14 12:00:00", NAN, 51.5, 1012.5, NAN, NAN);
    const std::optional<EnvironmentalReading> back = EnvironmentalReading::fromJson(original.toJson());
    assertTrue((bool)back);
    assertEqual(back->timestamp.c_str(), "2026-11-14 12:00:00");
    assertTrue(std::isnan(back->temperature));
    assertNear(back->humidity, 51.5, 1e-4);
    assertNear(back->pressure, 1012.5, 1e-4);
    assertTrue(std::isnan(back->dewpoint));
}

test(Reading_FromJsonRejectsGarbage) {
    assertFalse((bool)EnvironmentalReading::fromJson("not json"));
    assertFalse((bool)EnvironmentalReading::fromJson("{\"temperature\":20}"));
}
