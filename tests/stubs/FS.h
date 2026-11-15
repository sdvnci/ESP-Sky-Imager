#pragma once
// In-memory stand-in for the ESP32 core's fs::FS / fs::File, covering what the station uses.
#include <Arduino.h>
#include <map>
#include <memory>
#include <string>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

namespace fs {
    struct Disk {
        std::map<std::string, std::string> files;
        bool failWrites = false;
        bool failRename = false;
    };

    class File : public Stream {
        public:
            File() = default;
            File(Disk* disk, std::string path, std::string mode) : _disk(disk), _path(std::move(path)), _mode(std::move(mode)) {
                if (_mode == "w") _disk->files[_path] = "";
                else if (_mode == "a") _disk->files[_path];
            }
            explicit operator bool() const { return _disk != nullptr; }
            void close() { _disk = nullptr; }
            bool isDirectory() const { return false; }
            size_t size() const { return _disk ? _disk->files[_path].size() : 0; }
            int available() override { return _disk && _mode == "r" ? (int)(_disk->files[_path].size() - _pos) : 0; }
            int read() override { return available() ? (uint8_t)_disk->files[_path][_pos++] : -1; }
            int peek() override { return available() ? (uint8_t)_disk->files[_path][_pos] : -1; }
            size_t write(uint8_t c) override {
                if (!_disk || _mode == "r" || _disk->failWrites) return 0;
                _disk->files[_path].push_back((char)c);
                return 1;
            }
            using Print::write;
        private:
            Disk* _disk = nullptr;
            std::string _path, _mode;
            size_t _pos = 0;
    };

    class FS {
        public:
            Disk disk;
            File open(const char* path, const char* mode = FILE_READ, bool create = false) {
                const std::string m(mode);
                if (m == "r" && !disk.files.count(path)) return File();
                if (m != "r" && !create && !disk.files.count(path) && m != "w" && m != "a") return File();
                return File(&disk, path, m);
            }
            bool exists(const char* path) { return disk.files.count(path) > 0; }
            bool remove(const char* path) { return disk.files.erase(path) > 0; }
            bool rename(const char* from, const char* to) {
                if (disk.failRename || !disk.files.count(from) || disk.files.count(to)) return false;
                disk.files[to] = disk.files[from];
                disk.files.erase(from);
                return true;
            }
    };
};
using fs::File;
