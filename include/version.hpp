#pragma once
#include <string>
#include <string_view>

namespace version {
    constexpr std::string_view CURRENT_VERSION = "0.1.0";
    
    bool checkForUpdates();
    std::string getLatestVersion();
    bool isUpdateAvailable();
    
    // Compare version strings (e.g., "1.2.3" > "1.2.0")
    bool isVersionNewer(const std::string& version1, const std::string& version2);
} 