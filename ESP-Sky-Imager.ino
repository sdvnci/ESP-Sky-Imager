#include "io.hpp"
#include "communications.hpp"
#include "SensorContainer.hpp"

/**
 * @brief Parse the server's QNH response, eg: {"qnh": 1013}.
 * @param json The response body.
 * @return QNH in hPa, or NaN if the response has none.
 */
float parseQnh(const std::string& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) return NAN;
    return doc["qnh"] | NAN;
}

void setup() {
    Serial.begin(115200);
    while(!Serial) {
        delay(100);
    }

    debugln("Starting ESP-Sky-Imager...");

    fs::FS& fs = determineFileSystem();
    
    SensorContainer sense;
    if (!sense.begin()) {
        debugln("No environmental sensors found.");
    }

    WifiSpec intfce = WifiSpec();
    intfce.wifiConnectionSetup(fs, sense.status);

    // we should be connected to WiFi now
    intfce.setClock();
    ServerInfo server = ServerInfo::fromFile(fs);

    intfce.getInternalTime(20);
    server.websiteReachable();

    // QNH first, so the reading can include altitude.
    const float qnh = parseQnh(server.getQnh(intfce));
    const EnvironmentalReading reading = sense.read(formatTime(intfce.timeinfo), qnh);
    debugf("Reading: %s\n", reading.toJson().c_str());

    server.sendStatuses(intfce, sense.status);
    server.sendReading(intfce, reading);
    server.getFirmwareVersion(intfce);
}


void loop(){
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

