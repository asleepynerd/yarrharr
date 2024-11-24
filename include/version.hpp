#pragma once
#include <string>
#include <string_view>

namespace version {
    constexpr std::string_view CURRENT_VERSION = "2.1.0";

    bool checkForUpdates();
    std::string getLatestVersion();
    bool isUpdateAvailable();
    
    bool isVersionNewer(const std::string& version1, const std::string& version2);
} 