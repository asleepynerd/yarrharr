#include "utils.hpp"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace utils {

std::string sanitizeFilename(const std::string& filename) {
    std::string result = filename;
    const std::string invalid_chars = "\\/:*?\"<>|";
    
    for (char c : invalid_chars) {
        result.erase(std::remove(result.begin(), result.end(), c), result.end());
    }
    
    return result;
}

std::string formatFileSize(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return ss.str();
}

void showProgressBar(int progress, int total) {
    const int bar_width = 50;
    float percentage = (float)progress / total;
    int pos = bar_width * percentage;
    
    std::cout << "[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << int(percentage * 100.0) << "%\r";
    std::cout.flush();
}

std::vector<std::string> splitString(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool createDirectoryIfNotExists(const std::string& path) {
    try {
        return fs::create_directories(path);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating directory: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> wrapText(const std::string& text, size_t width) {
    std::vector<std::string> lines;
    std::string line;
    std::string word;
    std::istringstream words(text);

    size_t lineLength = 0;
    while (words >> word) {
        if (lineLength + word.length() + 1 <= width) {
            if (!line.empty()) {
                line += " ";
                lineLength++;
            }
            line += word;
            lineLength += word.length();
        } else {
            if (!line.empty()) {
                lines.push_back(line);
            }
            line = word;
            lineLength = word.length();
        }
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

std::string padNumber(int num, int width) {
    std::ostringstream ss;
    ss << std::setw(width) << std::setfill('0') << num;
    return ss.str();
}

}