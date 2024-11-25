#include "config.hpp"
#include <fstream>
#include <filesystem>

Config Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Config{
            "f6e840332142f77746185ab4e67be858",     
            "sleepy_xxxxxxxxxxxxxxxxxxxxxxxxx",     
            "downloads", 
            true,   
            true    
        };
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
    nlohmann::json j;
    j["tmdb_api_key"] = tmdb_api_key;
    j["yarrharr_api_key"] = yarrharr_api_key;
    j["download_path"] = download_path;
    j["create_season_folders"] = create_season_folders;
    j["create_show_folders"] = create_show_folders;
    
    std::ofstream file(path);
    file << j.dump(4);
}