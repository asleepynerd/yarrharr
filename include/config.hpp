#pragma once
#include <string>
#include <nlohmann/json.hpp>

struct Config {
    std::string tmdb_api_key;
    std::string yarrharr_api_key;
    std::string download_path;
    bool create_season_folders;
    bool create_show_folders;
    
    static Config load(const std::string& path);
    void save(const std::string& path) const;
    
    static std::string getConfigPath();
    static void ensureConfigDirectory();
};