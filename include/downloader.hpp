#pragma once
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <set>
#include "tmdb.hpp"

class Downloader {
public:
    explicit Downloader(const std::string& base_url, bool mp4_mode = false, bool skip_specials = false);
    
    void setApiKey(const std::string& api_key) { api_key_ = api_key; }
    void downloadMovie(const Movie& movie, const std::string& output_dir);
    void downloadEpisode(const Show& show, const Episode& episode, const std::string& output_dir);
    void downloadSeason(const Show& show, int season, const std::string& output_dir);
    void downloadShow(const Show& show, const std::string& output_dir);
    
    void setProgressCallback(std::function<void(int, int)> callback);
    void downloadFile(const std::string& url, const std::string& output_path);
    void parseProgress(const std::string& line);

private:
    std::string base_url_;
    std::string api_key_;
    bool mp4_mode_;
    bool skip_specials_;
    std::function<void(int, int)> progress_callback_;
    
    std::string buildUrl(const std::string& tmdb_id, int season = 0, int episode = 0);
};