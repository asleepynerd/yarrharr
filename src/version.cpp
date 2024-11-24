#include "version.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <regex>

namespace {
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string fetchLatestRelease() {
        CURL* curl = curl_easy_init();
        std::string readBuffer;
        
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, 
                "https://api.github.com/repos/asleepynerd/yarrharr/releases/latest");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "YarrHarr-Version-Check");
            
            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            
            if (res != CURLE_OK) {
                return "";
            }
        }
        
        return readBuffer;
    }
}

namespace version {
    bool isVersionNewer(const std::string& version1, const std::string& version2) {
        std::regex version_regex(R"((\d+)\.(\d+)\.(\d+))");
        std::smatch match1, match2;
        
        if (!std::regex_match(version1, match1, version_regex) ||
            !std::regex_match(version2, match2, version_regex)) {
            return false;
        }
        
        for (size_t i = 1; i <= 3; ++i) {
            int v1 = std::stoi(match1[i].str());
            int v2 = std::stoi(match2[i].str());
            if (v1 != v2) {
                return v1 > v2;
            }
        }
        
        return false;
    }

    std::string getLatestVersion() {
        try {
            std::string response = fetchLatestRelease();
            if (response.empty()) {
                return std::string(CURRENT_VERSION);
            }
            
            auto json = nlohmann::json::parse(response);
            std::string tag_name = json["tag_name"];
            
            if (!tag_name.empty() && tag_name[0] == 'v') {
                tag_name = tag_name.substr(1);
            }
            
            return tag_name;
            
        } catch (const std::exception& e) {
            return std::string(CURRENT_VERSION);
        }
    }

    bool isUpdateAvailable() {
        std::string latestVersion = getLatestVersion();
        return isVersionNewer(latestVersion, std::string(CURRENT_VERSION));
    }

    bool checkForUpdates() {
        try {
            std::string latestVersion = getLatestVersion();
            
            if (isVersionNewer(latestVersion, std::string(CURRENT_VERSION))) {
                std::cout << "\nUpdate available! "
                          << "Current version: " << CURRENT_VERSION
                          << ", Latest version: " << latestVersion << "\n"
                          << "Visit https://github.com/asleepynerd/yarrharr/releases "
                          << "to download the latest version.\n\n";
                return true;
            }
            
        } catch (const std::exception& e) {
        }
        
        return false;
    }
} 