#include "config.hpp"
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

std::string Config::getConfigPath() {
    const char* home = getenv("HOME");
    if (!home) {
        throw std::runtime_error("Could not determine home directory");
    }
    
    return (fs::path(home) / ".yarrharr" / "config.json").string();
}

void Config::ensureConfigDirectory() {
    const char* home = getenv("HOME");
    if (!home) {
        throw std::runtime_error("Could not determine home directory");
    }
    
    fs::path config_dir = fs::path(home) / ".yarrharr";
    if (!fs::exists(config_dir)) {
        fs::create_directories(config_dir);
    }
}

Config Config::load(const std::string& path) {
    ensureConfigDirectory();
    
    std::ifstream file(path);
    if (!file.is_open()) {
        // Create default config
        Config default_config{
            "f6e840332142f77746185ab4e67be858",     
            "sleepy_xxxxxxxxxxxxxxxxxxxxxxxxx",     
            (fs::path(getenv("HOME")) / "Downloads").string(), 
            true,   
            true    
        };
        
        // Save default config
        default_config.save(path);
        return default_config;
    }
    
    nlohmann::json j;
    file >> j;
    
    return Config{
        j["tmdb_api_key"].get<std::string>(),
        j["yarrharr_api_key"].get<std::string>(),
        j["download_path"].get<std::string>(),
        j["create_season_folders"].get<bool>(),
        j["create_show_folders"].get<bool>()
    };
}

void Config::save(const std::string& path) const {
    ensureConfigDirectory();
    
    nlohmann::json j;
    j["tmdb_api_key"] = tmdb_api_key;
    j["yarrharr_api_key"] = yarrharr_api_key;
    j["download_path"] = download_path;
    j["create_season_folders"] = create_season_folders;
    j["create_show_folders"] = create_show_folders;
    
    std::ofstream file(path);
    file << j.dump(4);
}