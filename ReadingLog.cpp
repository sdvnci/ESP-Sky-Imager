#include "ReadingLog.hpp"


void ReadingLog::recover(void) {
    const std::string pending = pendingPath();
    if (!_fs.exists(pending.c_str())) return;

    if (_fs.exists(_path.c_str())) {
        _fs.remove(pending.c_str());
    } else if (!_fs.rename(pending.c_str(), _path.c_str())) {
        debugf("Failed to recover %s.\n", pending.c_str());
    }
}


bool ReadingLog::append(const EnvironmentalReading& reading) {
    recover();

    // fs::File closes itself when it goes out of scope.
    File file = _fs.open(_path.c_str(), FILE_APPEND, true);
    if (!file) {
        debugf("Failed to open %s for appending.\n", _path.c_str());
        return false;
    }

    const std::string line = reading.toJson();
    const bool written = file.println(line.c_str()) == line.size() + 2;
    if (!written) debugf("Failed to append reading to %s.\n", _path.c_str());
    return written;
}


bool ReadingLog::empty(void) const {
    if (!_fs.exists(_path.c_str())) return true;
    File file = _fs.open(_path.c_str(), FILE_READ);
    return !file || file.size() == 0;
}


ReadingLog::ReplayResult ReadingLog::replay(
    const std::function<Delivery(const EnvironmentalReading&)>& send,
    size_t maxSends
) {
    recover();

    ReplayResult result;
    if (empty()) return result;

    const std::string pending = pendingPath();
    bool writeFailed = false;

    {
        File in = _fs.open(_path.c_str(), FILE_READ);
        File out = _fs.open(pending.c_str(), FILE_WRITE, true);
        if (!in || !out) {
            debugf("Can't replay %s: failed to open it or %s.\n", _path.c_str(), pending.c_str());
            return result;
        }

        auto keep = [&](const String& line) {
            if (out.println(line) != line.length() + 2) writeFailed = true;
            ++result.kept;
        };

        bool stopped = false;
        while (in.available()) {
            String line = in.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;

            if (stopped || result.sent + result.rejected >= maxSends) {
                keep(line);
                continue;
            }

            const std::optional<EnvironmentalReading> reading = EnvironmentalReading::fromJson(line.c_str());
            if (!reading) {
                debugf("Dropping unreadable log line: %s\n", line.c_str());
                ++result.rejected;
                continue;
            }

            switch (send(*reading)) {
                case Delivery::SENT:
                    ++result.sent;
                    break;
                case Delivery::REJECTED:
                    debugf("Server rejected logged reading from %s - dropping it.\n", reading->timestamp.c_str());
                    ++result.rejected;
                    break;
                case Delivery::FAILED:
                    keep(line);
                    stopped = true;
                    break;
            }
        }
    }   // Both files are closed here, before the swap.

    if (writeFailed) {
        // The originals are still in the log; re-sending a few delivered ones beats losing data.
        debugf("Failed writing %s - keeping %s as it was.\n", pending.c_str(), _path.c_str());
        _fs.remove(pending.c_str());
        return result;
    }

    _fs.remove(_path.c_str());
    if (result.kept == 0) {
        _fs.remove(pending.c_str());
    } else if (!_fs.rename(pending.c_str(), _path.c_str())) {
        debugf("Failed to move %s to %s.\n", pending.c_str(), _path.c_str());
    }

    debugf("Log replay: %u sent, %u rejected, %u kept.\n",
           (unsigned)result.sent, (unsigned)result.rejected, (unsigned)result.kept);
    return result;
}
