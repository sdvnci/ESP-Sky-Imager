#include <AUnit.h>
#include "SensorContainer.hpp"
#include "FakeDevices.hpp"

namespace {
    /** BMP390 calibration with known reference outputs (see the drivers' tests). */
    const std::array<uint8_t, 21> CALIBRATION = {
        0x6C, 0x6B, 0x38, 0x4A, 0xF9, 0x30, 0x75, 0x3C, 0xF6, 0x23, 0x00,
        0xDB, 0x03, 0x48, 0x71, 0x03, 0xFA, 0x68, 0x42, 0x08, 0xC4
    };
    constexpr uint32_t RAW_TEMPERATURE = 8450000;   // 24.9007 °C
    constexpr uint32_t RAW_PRESSURE = 6600000;      // 101332.87 Pa
    constexpr float PRESSURE_HPA = 1013.3287f;

    /** A bus with both sensors answering. */
    struct Station {
        FakeSHT31 sht;
        FakeBMP3 bmp{bmp3::CHIP_ID_BMP390, CALIBRATION};

        Station() {
            resetFakeBuses();
            bmp.rawTemperature = RAW_TEMPERATURE;
            bmp.rawPressure = RAW_PRESSURE;
        }

        void attachSht() { wire()->attach(sht3x::ADDRESS_PRIMARY, &sht); }
        void attachBmp() { wire()->attach(bmp3::ADDRESS_SECONDARY, &bmp); }
        FakeWire* wire() { return I2CManager::getInstance().wire(sensorpins::I2C_BUS); }
    };
}


test(Sensors_BeginBringsUpBoth) {
    Station station;
    station.attachSht();
    station.attachBmp();
    SensorContainer sense;

    assertTrue(sense.begin());
    assertTrue(sense.status.SHT);
    assertTrue(sense.status.BMP);
    assertEqual(station.wire()->sda(), (int)sensorpins::I2C_SDA);
    // Weather settings: x8 pressure oversampling, IIR coefficient 3.
    assertEqual(station.bmp.lastWrite(bmp3::REG::OSR), 0x03);
    assertEqual(station.bmp.lastWrite(bmp3::REG::CONFIG), 0x02 << 1);
}


test(Sensors_ReadCombinesBoth) {
    Station station;
    station.attachSht();
    station.attachBmp();
    SensorContainer sense;
    assertTrue(sense.begin());

    const EnvironmentalReading r = sense.read("2026-11-14 12:00:00", PRESSURE_HPA);
    assertEqual(r.timestamp.c_str(), "2026-11-14 12:00:00");
    assertNear(r.temperature, 25.0, 0.01);          // from the SHT31
    assertNear(r.humidity, 50.0, 0.01);
    assertNear(r.pressure, (double)PRESSURE_HPA, 0.001);   // sent in hPa
    assertNear(r.altitude, 0.0, 0.05);              // QNH == station pressure
    assertNear(r.dewpoint, 13.85, 0.01);
    assertEqual(station.sht.measurements, (int)READING_SAMPLES);
}


test(Sensors_NoQnhMeansNoAltitude) {
    Station station;
    station.attachSht();
    station.attachBmp();
    SensorContainer sense;
    assertTrue(sense.begin());

    const EnvironmentalReading r = sense.read("t", NAN);
    assertTrue(std::isnan(r.altitude));
    assertNear(r.pressure, (double)PRESSURE_HPA, 0.001);
}


test(Sensors_ShtDownFallsBackToBmpTemperature) {
    Station station;
    station.attachBmp();
    SensorContainer sense;

    assertTrue(sense.begin());
    assertFalse(sense.status.SHT);
    assertTrue(sense.status.BMP);

    const EnvironmentalReading r = sense.read("t", 1013.25f);
    assertNear(r.temperature, 24.9007, 0.001);
    assertTrue(std::isnan(r.humidity));
    assertTrue(std::isnan(r.dewpoint));
    assertTrue(r.toJson().find("\"humidity\":null") != std::string::npos);
}


test(Sensors_NoneFound) {
    Station station;
    SensorContainer sense;

    assertFalse(sense.begin());
    assertFalse(sense.status.SHT);
    assertFalse(sense.status.BMP);

    const EnvironmentalReading r = sense.read("t", 1013.25f);
    assertTrue(std::isnan(r.temperature));
    assertTrue(std::isnan(r.pressure));
}
