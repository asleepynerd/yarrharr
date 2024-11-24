#include "config.hpp"
#include <fstream>
#include <filesystem>

Config Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Config{
            "",     // tmdb_api_key
            "",     // yarrharr_api_key
            "downloads", // download_path
            true,   // create_season_folders
            true    // create_show_folders
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