#include <AUnit.h>
#include "ReadingLog.hpp"
#include <algorithm>
#include <functional>
#include <vector>

namespace {
    EnvironmentalReading sample(const char* ts, double temp = 25.0) {
        return EnvironmentalReading(ts, temp, 50.0, 1013.25, 12.0, 13.86);
    }

    /** Replay into a vector, answering with the given outcomes in turn (then SENT). */
    struct Recorder {
        std::vector<std::string> timestamps;
        std::vector<ReadingLog::Delivery> answers;
        ReadingLog::Delivery operator()(const EnvironmentalReading& r) {
            timestamps.push_back(r.timestamp);
            if (answers.empty()) return ReadingLog::Delivery::SENT;
            const ReadingLog::Delivery d = answers.front();
            answers.erase(answers.begin());
            return d;
        }
    };

    size_t lines(fs::FS& fs, const char* path) {
        if (!fs.exists(path)) return 0;
        const std::string& s = fs.disk.files[path];
        return std::count(s.begin(), s.end(), '\n');
    }
}


test(Log_AppendThenReplayAll) {
    fs::FS fs;
    ReadingLog log(fs, "/r.jsonl");
    assertTrue(log.empty());
    assertTrue(log.append(sample("t1")));
    assertTrue(log.append(sample("t2")));
    assertTrue(log.append(sample("t3")));
    assertEqual(lines(fs, "/r.jsonl"), (size_t)3);

    Recorder rec;
    const ReadingLog::ReplayResult r = log.replay(std::ref(rec), 50);
    assertEqual(r.sent, (size_t)3);
    assertEqual(r.kept, (size_t)0);
    assertEqual(rec.timestamps.size(), (size_t)3);
    assertEqual(rec.timestamps[0].c_str(), "t1");   // oldest first
    assertEqual(rec.timestamps[2].c_str(), "t3");
    assertTrue(log.empty());
    assertFalse(fs.exists("/r.jsonl.tmp"));
}

test(Log_FailureKeepsRestInOrder) {
    fs::FS fs;
    ReadingLog log(fs, "/r.jsonl");
    for (const char* ts : {"t1", "t2", "t3", "t4"}) assertTrue(log.append(sample(ts)));

    Recorder first;
    first.answers = {ReadingLog::Delivery::SENT, ReadingLog::Delivery::FAILED};
    const ReadingLog::ReplayResult r = log.replay(std::ref(first), 50);
    assertEqual(r.sent, (size_t)1);
    assertEqual(r.kept, (size_t)3);
    assertEqual(first.timestamps.size(), (size_t)2);    // stopped after the failure

    Recorder second;
    log.replay(std::ref(second), 50);
    assertEqual(second.timestamps.size(), (size_t)3);
    assertEqual(second.timestamps[0].c_str(), "t2");
    assertEqual(second.timestamps[2].c_str(), "t4");
    assertTrue(log.empty());
}

test(Log_RejectedIsDropped) {
    fs::FS fs;
    ReadingLog log(fs, "/r.jsonl");
    assertTrue(log.append(sample("bad")));
    assertTrue(log.append(sample("good")));

    Recorder rec;
    rec.answers = {ReadingLog::Delivery::REJECTED};
    const ReadingLog::ReplayResult r = log.replay(std::ref(rec), 50);
    assertEqual(r.rejected, (size_t)1);
    assertEqual(r.sent, (size_t)1);
    assertTrue(log.empty());
}

test(Log_MaxSendsKeepsTheRest) {
    fs::FS fs;
    ReadingLog log(fs, "/r.jsonl");
    for (const char* ts : {"t1", "t2", "t3"}) assertTrue(log.append(sample(ts)));

    Recorder rec;
    const ReadingLog::ReplayResult r = log.replay(std::ref(rec), 2);
    assertEqual(r.sent, (size_t)2);
    assertEqual(r.kept, (size_t)1);
    assertEqual(lines(fs, "/r.jsonl"), (size_t)1);
}

test(Log_CorruptLineDroppedOthersKept) {
    fs::FS fs;
    ReadingLog log(fs, "/r.jsonl");
    assertTrue(log.append(sample("t1")));
    fs.disk.files["/r.jsonl"] += "{\"broken\r\n";
    assertTrue(log.append(sample("t2")));

    Recorder rec;
    const ReadingLog::ReplayResult r = log.replay(std::ref(rec), 50);
    assertEqual(r.sent, (size_t)2);
    assertEqual(r.rejected, (size_t)1);
}

test(Log_WriteFailureDuringReplayKeepsOriginal) {
    fs::FS fs;
    ReadingLog log(fs, "/r.jsonl");
    for (const char* ts : {"t1", "t2", "t3"}) assertTrue(log.append(sample(ts)));
    const std::string before = fs.disk.files["/r.jsonl"];

    fs.disk.failWrites = true;
    Recorder rec;
    rec.answers = {ReadingLog::Delivery::SENT, ReadingLog::Delivery::FAILED};
    log.replay(std::ref(rec), 50);

    assertTrue(fs.disk.files["/r.jsonl"] == before);
    assertFalse(fs.exists("/r.jsonl.tmp"));
}

test(Log_AppendFailureReported) {
    fs::FS fs;
    ReadingLog log(fs, "/r.jsonl");
    fs.disk.failWrites = true;
    assertFalse(log.append(sample("t1")));
}

test(Log_RecoversFromInterruptedSwap) {
    fs::FS fs;
    ReadingLog log(fs, "/r.jsonl");
    for (const char* ts : {"t1", "t2"}) assertTrue(log.append(sample(ts)));

    // Rename fails after the old log was removed - as if power dropped mid-swap.
    fs.disk.failRename = true;
    Recorder first;
    first.answers = {ReadingLog::Delivery::FAILED};
    log.replay(std::ref(first), 50);
    assertFalse(fs.exists("/r.jsonl"));
    assertTrue(fs.exists("/r.jsonl.tmp"));

    // Next wake: the pending file is recovered before the new reading is appended.
    fs.disk.failRename = false;
    assertTrue(log.append(sample("t3")));
    Recorder second;
    log.replay(std::ref(second), 50);
    assertEqual(second.timestamps.size(), (size_t)3);
    assertEqual(second.timestamps[0].c_str(), "t1");
    assertEqual(second.timestamps[2].c_str(), "t3");
}

test(Log_StaleTmpDroppedWhenLogIntact) {
    fs::FS fs;
    ReadingLog log(fs, "/r.jsonl");
    assertTrue(log.append(sample("t1")));
    fs.disk.files["/r.jsonl.tmp"] = sample("old").toJson() + "\r\n";

    Recorder rec;
    log.replay(std::ref(rec), 50);
    assertEqual(rec.timestamps.size(), (size_t)1);
    assertEqual(rec.timestamps[0].c_str(), "t1");
}
