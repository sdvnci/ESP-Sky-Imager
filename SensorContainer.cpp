#include "SensorContainer.hpp"

/**
 * @brief Initialise the SHT31 and BMP390 and record which came up in status.
 * @return true if at least one sensor is working.
 */
bool SensorContainer::begin(void) {
    status.SHT = sht.init();
    if (!status.SHT) {
        debugf("SHT31 init failed: %s\n", toString(sht.lastError()));
    }

    // Bosch's recommended weather-monitoring settings, applied by init().
    (void)bmp.configure(bmp3::Oversampling::X8, bmp3::Oversampling::X1, bmp3::IIRFilter::COEF_3);
    status.BMP = bmp.init();
    if (!status.BMP) {
        debugf("BMP3xx init failed: %s\n", toString(bmp.lastError()));
    }

    return status.SHT || status.BMP;
}


/**
 * @brief Take a reading, averaging READING_SAMPLES samples per sensor with outliers removed.
 * @param timestamp Timestamp to stamp the reading with.
 * @param qnhHpa Sea-level pressure in hPa for the altitude, or NaN to skip altitude.
 * @return The reading. Fields from sensors that are down or failed every sample are NaN.
 */
EnvironmentalReading SensorContainer::read(const std::string& timestamp, float qnhHpa) {
    std::vector<float> shtTemperatures, humidities, bmpTemperatures, pressures;
    shtTemperatures.reserve(READING_SAMPLES);
    humidities.reserve(READING_SAMPLES);
    bmpTemperatures.reserve(READING_SAMPLES);
    pressures.reserve(READING_SAMPLES);

    for (uint16_t i = 0; i < READING_SAMPLES; ++i) {
        if (status.SHT && sht.update()) {
            const sht3x::Reading r = sht.reading();
            shtTemperatures.push_back(r.temperature);
            humidities.push_back(r.humidity);
        }
        if (status.BMP && bmp.update()) {
            const bmp3::Reading r = bmp.reading();
            bmpTemperatures.push_back(r.temperature);
            pressures.push_back(r.pressure);
        }
    }

    if (status.SHT && shtTemperatures.empty()) {
        debugf("SHT31 read failed: %s\n", toString(sht.lastError()));
    }
    if (status.BMP && pressures.empty()) {
        debugf("BMP3xx read failed: %s\n", toString(bmp.lastError()));
    }

    // robustMean() of an empty set is NaN, which flows through to every derived value.
    const float temperature = shtTemperatures.empty()
        ? stats::robustMean(bmpTemperatures)
        : stats::robustMean(shtTemperatures);
    const float humidity = stats::robustMean(humidities);
    const float pressurePa = stats::robustMean(pressures);
    const float altitude = std::isnan(qnhHpa) ? NAN : env::altitude(pressurePa, qnhHpa);
    const float dewpoint = env::dewPoint(temperature, humidity);

    // The server stores pressure in hPa.
    return EnvironmentalReading(timestamp, temperature, humidity, pressurePa / 100.0f, altitude, dewpoint);
}
