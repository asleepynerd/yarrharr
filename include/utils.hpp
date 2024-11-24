#pragma once
#include <string>
#include <vector>

namespace utils {
    std::string sanitizeFilename(const std::string& filename);
    std::string formatFileSize(size_t bytes);
    void showProgressBar(int progress, int total);
    std::vector<std::string> splitString(const std::string& str, char delim);
    bool createDirectoryIfNotExists(const std::string& path);
    std::vector<std::string> wrapText(const std::string& text, size_t width);
    std::string padNumber(int num, int width);
}