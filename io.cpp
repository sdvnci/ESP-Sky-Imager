#include "io.hpp"


/**
 * @brief Format the current time into a string.
 * @param now Reference to a tm structure containing the current time.
 * @return A formatted string representing the current time.
 */
std::string formatTime(const tm& now) {
    // "YYYY-MM-DD HH:MM:SS" + null terminator
    char timestamp[35];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &now);
    return std::string(timestamp);
}


/**
 * Attempt to initialize the sdcard file system. 
 * @return True if the sdcard was successfully mounted, false otherwise.
 */
bool sdmmcInit(void){
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
      debugln("Card Mount Failed");
      return false;
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        debugln("No SD_MMC card attached");
        return false;
    }

    return true;
}


/**
 * Determine the file system to use based on the SD card status.
 * @return The file system to use for file operations.
 */
fs::FS& determineFileSystem(void) {
    if(sdmmcInit()) {
        debugln("SD_MMC mounted");
        return SD_MMC;
    }
    
    LittleFS.begin(true);
    debugln("LittleFS mounted");
    return LittleFS;
}


/**
 * @brief   Read the contents of a file from the specified file system.
 * @warning This dynamically allocates memory for the file contents - for our use case, this is fine though.
 * @param   fs The file system to read from.
 * @param   path The path to the file to read.
 * @return  The contents of the file as a string.
 */
std::string readFile(fs::FS& fs, const std::string& path) {
    debugf("\nReading file: %s\r\n", path.c_str());
    
    File file = fs.open(path.c_str(), FILE_READ);
    if(!file || file.isDirectory()){
        debugf("Failed to open file %s for reading\n", path.c_str());
        return "";
    }

    size_t fileSize = file.size();
    std::string content;
    content.reserve(fileSize);
    
    while(file.available()) {
        content += (char)file.read();
    }
    file.close();

    debugf("Read %d bytes from file %s\n", content.size(), path.c_str());
    return content;
}




