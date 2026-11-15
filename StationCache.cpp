#include "StationCache.hpp"


StationCache::StationCache(fs::FS& fs, std::string path)
    : _fs(fs), _path(std::move(path)) {
    const std::string content = readFile(_fs, _path);
    if (content.empty()) return;

    JsonDocument doc;
    if (deserializeJson(doc, content)) {
        debugf("Ignoring unreadable cache file %s.\n", _path.c_str());
        return;
    }

    if (doc["qnh"]["value"].is<float>()) {
        _qnh = doc["qnh"]["value"].as<float>();
        _qnh_at = doc["qnh"]["at"] | (time_t)0;
    }
    _ntp_at = doc["ntp"]["at"] | (time_t)0;
}


StationCache::~StationCache() {
    if (!save()) debugf("Failed to save cache file %s.\n", _path.c_str());
}


bool StationCache::withinAge(time_t at, time_t now, uint32_t maxAgeSeconds) {
    return at != 0 && now >= at && (now - at) <= (time_t)maxAgeSeconds;
}


std::optional<float> StationCache::freshQnh(time_t now, uint32_t maxAgeSeconds) const {
    if (!_qnh || !withinAge(_qnh_at, now, maxAgeSeconds)) return std::nullopt;
    return _qnh;
}


void StationCache::setQnh(float qnhHpa, time_t now) {
    _qnh = qnhHpa;
    _qnh_at = now;
    _dirty = true;
}


bool StationCache::ntpFresh(time_t now, uint32_t maxAgeSeconds) const {
    return withinAge(_ntp_at, now, maxAgeSeconds);
}


void StationCache::setNtpSynced(time_t now) {
    _ntp_at = now;
    _dirty = true;
}


bool StationCache::save(void) {
    if (!_dirty) return true;

    JsonDocument doc;
    if (_qnh) {
        doc["qnh"]["value"] = *_qnh;
        doc["qnh"]["at"] = _qnh_at;
    }
    doc["ntp"]["at"] = _ntp_at;

    File file = _fs.open(_path.c_str(), FILE_WRITE, true);
    if (!file || serializeJson(doc, file) == 0) return false;

    _dirty = false;
    return true;
}
