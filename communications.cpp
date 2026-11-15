#include "communications.hpp"


/**
 * Connect to the WiFi network.
 * This function attempts to connect to the specified WiFi network using the provided SSID and password.
 * It will retry the connection a specified number of times before giving up.
 * 
 * @param ssid The SSID of the WiFi network to connect to.
 * @param password The password for the WiFi network.
 * @param status The status object to update with the connection status.
 * @param retryCount The number of times to retry the connection if it fails.
 * @return true if the connection was successful, false otherwise.
 */
[[nodiscard]]
bool WifiSpec::connect(
    const std::string& ssid,
    const std::string& password,
    SensorContainer::Status& status,
    uint8_t retryCount
) {
    debugf("Connecting to WiFi network: %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    uint8_t connectCount = 0;
    while (WiFi.status() != WL_CONNECTED && connectCount < retryCount) {
        vTaskDelay(250 / portTICK_PERIOD_MS);
        debug(".");
        connectCount++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        debugln("Failed to connect to WiFi.");
        status.WIFI = false;
        return false;
    }

    status.WIFI = true;
    debugf("Connected to WiFi. \nMAC: %s, \nIP: %s\n",
           WiFi.macAddress().c_str(),
           WiFi.localIP().toString().c_str()
    );

    this->ssid = ssid;
    this->password = password;
    this->isConnected = true;

    return true;
}


/**
 * @brief Read from the known networks json file and connect to the first matching network.
 * @param fs The filesystem to read the network information from.
 * @param status The status object to update with the connection status.
 * @return true if a network was found and connected to, false otherwise.
 */
bool WifiSpec::wifiConnectionSetup(fs::FS& fs, SensorContainer::Status& status) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect();

    const std::string nwinfo = readFile(fs, NETWORKS_FILE);
    if (nwinfo.empty()) {
        debugln("No network information found. Please set up the WiFi network.");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, nwinfo);
    if (error) {
        debugf("Failed to parse network file: %s\n", error.c_str());
        return false;
    }

    const JsonArray networks = doc["networks"];
    int netcount = WiFi.scanNetworks();

    for (int i = 0; i < netcount; i++) {
        std::string ssid = std::string(WiFi.SSID(i).c_str());
        for (JsonVariant netJson : networks) {
            std::string netssid = netJson["ssid"];
            std::string netpwd = netJson["pass"];
            if (ssid == netssid) {
                if (connect(netssid, netpwd, status, RETRY_COUNT)) {
                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * Get the current time from the onboard clock, in UTC.
 * This function will keep trying to get the time until it succeeds or the timer runs out.
 * It will print a dot every 150-250 milliseconds to indicate progress.
 * If the time is not set after the timer runs out, it will print an error message.
 * @param timer: The maximum time to wait for the time to be set, in seconds.
 * @return true if the time was successfully set, false otherwise.
 */
bool WifiSpec::getInternalTime(int timer) {
    uint32_t start = millis();
    time_t now;
    debug("Getting time!");
  
    do {
      time(&now);
      gmtime_r(&now, &timeinfo);
      if (timeinfo.tm_year >= 71) break;  // 71 = year 1971
      debug(".");
      vTaskDelay(random(150, 250)/portTICK_PERIOD_MS);
    } while ((millis() - start) <= (uint32_t)(1000 * timer));
    debugln("Done");
    
    if (timeinfo.tm_year < 71) {
        debugln("Failed to get time from NTP server.");
        return false;
    }

    debugf("Current time: %s\n", asctime(&timeinfo));
    return true;
}

/**
 * Set the internal clock of the ESP32 to the current time using NTP AND fill the timeinfo struct with that time.
 * Big thanks to Andreas Spiess.
 * @param timeoutMs How long to wait for the NTP server before giving up.
 * @return true if the clock was set, false on timeout.
 */
bool WifiSpec::setClock(uint32_t timeoutMs) {
    configTime(0, 0, "pool.ntp.org");
    debug(F("Waiting for NTP time sync: "));

    const uint32_t start = millis();
    time_t nowSecs = time(nullptr);
    while (nowSecs < 1577836800) {  // January 1, 2020 - reasonable minimum
        if (millis() - start > timeoutMs) {
            debugln(F(" timed out."));
            return false;
        }
        vTaskDelay(500/portTICK_PERIOD_MS);
        debug(F("."));
        nowSecs = time(nullptr);
    }
  
    debugln();
    gmtime_r(&nowSecs, &timeinfo);
    debug(F("Current time: "));
    debug(asctime(&timeinfo));
    return true;
}


namespace {
    /**
     * Owns an HTTPClient for one request, so end() runs however the request exits.
     */
    class HttpSession {
        public:
            explicit HttpSession(const std::string& url) {
                _client.begin(url.c_str());
                _client.setConnectTimeout(CONN_TIMEOUT);
                _client.setTimeout(READ_TIMEOUT);
            }

            ~HttpSession() { _client.end(); }

            HttpSession(const HttpSession&) = delete;
            HttpSession& operator=(const HttpSession&) = delete;

            HTTPClient& client(void) { return _client; }

        private:
            HTTPClient _client;
    };
}


/**
 * POST (when body is set) or GET a JSON request with the station's headers.
 * @param url The URL to send the request to.
 * @param body The JSON body to POST, or nullptr to GET.
 * @param timestamp Value for the X-Timestamp header. Empty means now.
 * @return The server's response, or a negative code if the request never completed.
 */
HttpResponse WifiSpec::request(const std::string& url, const std::string* body, const std::string& timestamp) {
    getInternalTime(5);

    HttpSession session(url);
    HTTPClient& webclient = session.client();

    webclient.addHeader(ServerInfo::Header::CONTENT_TYPE, ServerInfo::MIMEType::APP_JSON);
    webclient.addHeader(ServerInfo::Header::MACADDRESS, WiFi.macAddress().c_str());
    webclient.addHeader(ServerInfo::Header::TIMESTAMP, timestamp.empty() ? formatTime(timeinfo).c_str() : timestamp.c_str());
    webclient.addHeader(ServerInfo::Header::FIRMWARE_VERSION, VERSION);

    HttpResponse response;
    response.code = body ? webclient.POST(body->c_str()) : webclient.GET();

    if (response.code < 0) {
        debugf("Request to %s failed: %s\n", url.c_str(), webclient.errorToString(response.code).c_str());
        return response;
    }

    response.body = webclient.getString().c_str();
    return response;
}


/**
 * POST a JSON string to the server.
 * @param url The URL to send the JSON data to.
 * @param jsonData The JSON data to send.
 * @param timestamp Value for the X-Timestamp header - when the data was recorded. Empty means now.
 * @return The server's response.
 */
HttpResponse WifiSpec::sendJson(
    const std::string& url,
    const std::string& jsonData,
    const std::string& timestamp
) {
    return request(url, &jsonData, timestamp);
}


/**
 * Send a GET request to the given url expecting a JSON response.
 * @param url the url to send the request to.
 * @return The server's response.
 */
HttpResponse WifiSpec::getJson(const std::string& url) {
    return request(url, nullptr, "");
}


/**
 * Check if the server is reachable.
 * @param netIntf The network interface to use for the request.
 * @return true if the server is reachable, false otherwise.
 */
bool ServerInfo::websiteReachable(WifiSpec& netIntf) const {
    debugf("Checking if server is reachable at %s\n", url(Route::INDEX).c_str());

    const HttpResponse response = netIntf.getJson(url(Route::INDEX));
    if (response.code == 200) {
        debugln(">> Server is reachable.\n");
        return true;
    }

    debugf(">> Server is not reachable. HTTP code: %d\n\n", response.code);
    return false;
}


/**
 * Send the status of the sensors to the server.
 * @param netIntf The network interface to use for the request.
 * @param status The sensor status struct to send.
 * @return The server's response.
 */
HttpResponse ServerInfo::sendStatuses(
    WifiSpec& netIntf,
    const SensorContainer::Status& status
) const {
    debugf("Sending Statuses to %s\n", url(Route::STATUS).c_str());
    const HttpResponse response = netIntf.sendJson(url(Route::STATUS), status.toJson());
    debugf("Response %d: %s\n", response.code, response.body.c_str());
    return response;
}


/**
 * Send a reading to the server, stamped with the time it was taken.
 * @param netIntf The network interface to use for the request.
 * @param envdata The environmental data to send.
 * @return The server's response.
 */
HttpResponse ServerInfo::sendReading(
    WifiSpec& netIntf,
    const EnvironmentalReading& envdata
) const {
    debugf("Sending Reading to %s\n", url(Route::READING).c_str());
    // The server takes the reading's time from the header, so a logged
    // reading sent later must carry its own timestamp, not the send time.
    const HttpResponse response = netIntf.sendJson(url(Route::READING), envdata.toJson(), envdata.timestamp);
    debugf("Response %d: %s\n", response.code, response.body.c_str());
    return response;
}


/**
 * Get the QNH from the server.
 * @param netInf The network interface settings to use for the request.
 * @return QNH in hPa, or nothing if the server didn't provide one.
 */
std::optional<float> ServerInfo::fetchQnh(WifiSpec& netInf) const {
    debugf("Getting QNH from %s\n", url(Route::QNH).c_str());
    const HttpResponse response = netInf.getJson(url(Route::QNH));
    debugf("Response %d: %s\n", response.code, response.body.c_str());
    if (!response.ok()) return std::nullopt;

    JsonDocument doc;
    if (deserializeJson(doc, response.body)) return std::nullopt;

    const JsonVariant qnh = doc["qnh"];
    if (!qnh.is<float>()) return std::nullopt;
    return qnh.as<float>();
}


/**
 * Get the firmware version from the server as JSON.
 * @param netInf The network interface to use for the request.
 * @return The server's response.
 */
HttpResponse ServerInfo::getFirmwareVersion(WifiSpec& netInf) const {
    debugf("Getting firmware version from %s\n", url(Route::VERSIONING).c_str());
    const HttpResponse response = netInf.getJson(url(Route::VERSIONING));
    debugf("Response %d: %s\n", response.code, response.body.c_str());
    return response;
}
