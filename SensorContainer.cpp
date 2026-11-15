#include "SensorContainer.hpp"

/**
 * @brief Write a new log entry to the log file.
 * @param fs The file system to use for the log.
 * @param reading The EnvironmentalReading object containing sensor data.
 * @return True if the log was successfully updated, false otherwise.
 */
[[nodiscard]]
bool writeLog(fs::FS& fs, const EnvironmentalReading& reading) {
    std::string logContent = readFile(fs, LOG_FILE);
    if (logContent.empty()) {
        debugln("Log file is empty or could not be read.");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, logContent);
    if (error) {
        debugf("Failed to parse log file: %s\n", error.c_str());
        return false;
    }

    JsonArray readings = doc["readings"];
    JsonObject newReading = readings.createNestedObject();
    
    newReading["timestamp"] = reading.timestamp;
    newReading["temperature"] = reading.temperature;
    newReading["humidity"] = reading.humidity;
    newReading["pressure"] = reading.pressure;
    newReading["altitude"] = reading.altitude;
    newReading["dewpoint"] = reading.dewpoint;

    File file = fs.open(LOG_FILE, FILE_WRITE);
    if (!file) {
        debugln("Failed to open log file for writing.");
        return false;
    }

    if (serializeJson(doc, file) == 0) {
        debugln("Failed to write updated log to file.");
        file.close();
        return false;
    }

    debugln("Log updated successfully.");
    file.close();

    return true;
}

/**
 * Convert a JsonArray to a vector of EnvironmentalReading objects.
 * @param jsonarray The JsonArray to convert.
 * @return A vector of EnvironmentalReading objects.
 */
std::vector<EnvironmentalReading> arrayFromJson(const JsonArray jsonarray) {
    std::vector<EnvironmentalReading> readings;
    readings.reserve(jsonarray.size());

    for (const auto& item : jsonarray) {

        EnvironmentalReading reading(
            item["timestamp"].as<std::string>(),
            item["temperature"].as<double>(),
            item["humidity"].as<double>(),
            item["pressure"].as<double>(),
            item["altitude"].as<double>(),
            item["dewpoint"].as<double>()
        );

        readings.push_back(reading);
    }

    return readings;
}


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
