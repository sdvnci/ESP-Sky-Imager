#pragma once
// Test stub - not part of the firmware build.
#include "FS.h"
class LittleFSFS : public fs::FS {
    public:
        bool begin(bool) { return true; }
};
extern LittleFSFS LittleFS;
