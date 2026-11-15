#pragma once
// Test stub - not part of the firmware build.
#include "FS.h"
#define CARD_NONE 0
#define SDMMC_FREQ_DEFAULT 20000
class SDMMCFS : public fs::FS {
    public:
        bool setPins(int, int, int) { return true; }
        bool begin(const char*, bool, bool, int, int) { return false; }
        uint8_t cardType() { return CARD_NONE; }
};
extern SDMMCFS SD_MMC;
