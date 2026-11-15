#pragma once

#include <ArduinoJson.h>
#include "FS.h"
#include <LittleFS.h>
#include "SD_MMC.h"
#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <time.h>

#define DEBUG 1
#define VERSION "1.0.0"

#ifdef DEBUG
    #define debug(...) Serial.print(__VA_ARGS__)
    #define debugln(...) Serial.println(__VA_ARGS__)
    #define debugf(...) Serial.printf(__VA_ARGS__)
#else
    #define debug(...)
    #define debugln(...)
    #define debugf(...)
#endif


#define SD_MMC_CMD  38
#define SD_MMC_CLK  39
#define SD_MMC_D0   40

#define LOG_FILE "/readings.jsonl"
#define CACHE_FILE "/cache.json"
#define SERVER_FILE "//server.json"
#define NETWORKS_FILE "//networks.json"

std::string formatTime(const tm& now);


/**
 * Attempt to initialize the sdcard file system. 
 * @return True if the sdcard was successfully mounted, false otherwise.
 */
bool sdmmcInit(void);


/**
 * Determine the file system to use based on the SD card status.
 * @return The file system to use for file operations.
 */
fs::FS& determineFileSystem(void);


/**
 * @brief   Read the contents of a file from the specified file system.
 * @warning This dynamically allocates memory for the file contents - for our use case, this is fine though.
 * @param   fs The file system to read from.
 * @param   path The path to the file to read.
 * @return  The contents of the file as a string.
 */
std::string readFile(fs::FS& fs, const std::string& path);
