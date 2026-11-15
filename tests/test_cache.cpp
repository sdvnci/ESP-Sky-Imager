#include <AUnit.h>
#include "StationCache.hpp"


test(Cache_EmptyHasNothing) {
    fs::FS fs;
    StationCache cache(fs, "/c.json");
    assertFalse((bool)cache.freshQnh(1000000, 7200));
    assertFalse((bool)cache.lastQnh());
    assertFalse(cache.ntpFresh(1000000, 21600));
}

test(Cache_SavedOnDestructionAndReloaded) {
    fs::FS fs;
    const time_t t0 = 1763100000;
    {
        StationCache cache(fs, "/c.json");
        cache.setQnh(1012.5f, t0);
        cache.setNtpSynced(t0);
    }
    assertTrue(fs.exists("/c.json"));

    StationCache cache(fs, "/c.json");
    assertNear(*cache.freshQnh(t0 + 3600, 7200), 1012.5f, 1e-4f);
    assertFalse((bool)cache.freshQnh(t0 + 7201, 7200));          // too old
    assertNear(*cache.lastQnh(), 1012.5f, 1e-4f);                // but still known
    assertTrue(cache.ntpFresh(t0 + 21600, 21600));
    assertFalse(cache.ntpFresh(t0 + 21601, 21600));
    assertFalse(cache.ntpFresh(t0 - 10, 21600));                 // clock went backwards
}

test(Cache_UnchangedIsNotWritten) {
    fs::FS fs;
    { StationCache cache(fs, "/c.json"); }
    assertFalse(fs.exists("/c.json"));
}

test(Cache_CorruptFileIgnored) {
    fs::FS fs;
    fs.disk.files["/c.json"] = "{not json";
    StationCache cache(fs, "/c.json");
    assertFalse((bool)cache.lastQnh());
}
