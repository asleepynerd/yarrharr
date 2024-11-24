#include "downloader.hpp"
#include "utils.hpp"
#include <curl/curl.h>
#include <filesystem>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <set>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cstdlib>

namespace fs = std::filesystem;

size_t writeCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (dltotal <= 0) return 0;
    
    static auto lastUpdate = std::chrono::steady_clock::now();
    static curl_off_t lastBytes = 0;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate);
    
    if (duration.count() >= 100) {  // 100ms interval
        std::cout << "\r";
        
        // Calculate progress
        int progress = static_cast<int>((dlnow * 100.0) / dltotal);
        
        // Calculate current speed
        double speed = (dlnow - lastBytes) * 1000.0 / duration.count(); // bytes per second
        double speedMB = speed / 1024.0 / 1024.0; // MB per second
        
        // Draw progress bar
        std::cout << "Progress: [";
        for (int i = 0; i < 50; i++) {
            if (i < progress / 2) {
                std::cout << "=";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "] " << progress << "%";
        
        // Show downloaded/total size and speed
        double downloadedMB = dlnow / 1024.0 / 1024.0;
        double totalMB = dltotal / 1024.0 / 1024.0;
        std::cout << " " << std::fixed << std::setprecision(2) 
                 << downloadedMB << "MB/" << totalMB << "MB"
                 << " @ " << speedMB << " MB/s";
        
        std::cout.flush();
        lastUpdate = now;
        lastBytes = dlnow;
    }
    
    return 0;
}

bool isFFmpegAvailable() {
    #ifdef _WIN32
    return system("where ffmpeg >nul 2>nul") == 0;
    #else
    return system("which ffmpeg >/dev/null 2>&1") == 0;
    #endif
}

bool convertToMp4(const std::string& input_path, const std::string& output_path) {
    std::string command = "ffmpeg -i \"" + input_path + "\" -c copy \"" + output_path + "\" -y";
    return system(command.c_str()) == 0;
}

Downloader::Downloader(const std::string& base_url, bool mp4_mode) : base_url_(base_url), mp4_mode_(mp4_mode) {
    if (mp4_mode_ && !isFFmpegAvailable()) {
        throw std::runtime_error("FFmpeg not found in PATH. Required for MP4 conversion.");
    }
}

void Downloader::downloadMovie(const Movie& movie, const std::string& output_dir) {
    std::string filename = utils::sanitizeFilename(movie.title + " (" + movie.release_date.substr(0, 4) + ").mkv");
    std::string output_path = (fs::path(output_dir) / "Movies" / filename).string();
    
    utils::createDirectoryIfNotExists((fs::path(output_dir) / "Movies").string());
    
    std::string url = buildUrl(movie.id);
    downloadFile(url, output_path);
}

void Downloader::downloadEpisode(const Show& show, const Episode& episode, const std::string& output_dir) {
    std::string show_dir = (fs::path(output_dir) / "TV Shows" / utils::sanitizeFilename(show.name)).string();
    
    std::string season_dir = (fs::path(show_dir) / 
        (std::string("Season ") + (episode.season < 10 ? "0" : "") + std::to_string(episode.season))).string();
    
    utils::createDirectoryIfNotExists(show_dir);
    utils::createDirectoryIfNotExists(season_dir);
    
    std::string filename = utils::sanitizeFilename(
        show.name + " - S" + 
        (episode.season < 10 ? "0" : "") + std::to_string(episode.season) + "E" + 
        (episode.episode < 10 ? "0" : "") + std::to_string(episode.episode) + " - " + 
        episode.name + ".mkv"
    );
    
    std::string output_path = (fs::path(season_dir) / filename).string();
    std::string url = buildUrl(show.id, episode.season, episode.episode);
    
    downloadFile(url, output_path);
}

void Downloader::downloadSeason(const Show& show, int season, const std::string& output_dir) {
    for (const auto& episode : show.episodes) {
        if (episode.season == season) {
            downloadEpisode(show, episode, output_dir);
        }
    }
}

void Downloader::downloadShow(const Show& show, const std::string& output_dir) {
    std::set<int, std::less<int>> seasons;
    for (const auto& episode : show.episodes) {
        seasons.insert(episode.season);
    }
    
    for (int season : seasons) {
        downloadSeason(show, season, output_dir);
    }
}

std::string Downloader::buildUrl(const std::string& tmdb_id, int season, int episode) {
    std::string url = base_url_ + "?tmdbId=" + tmdb_id;
    if (season > 0 && episode > 0) {
        url += "&season=" + std::to_string(season) + "&episode=" + std::to_string(episode);
    }
    return url;
}

void Downloader::downloadFile(const std::string& url, const std::string& output_path) {
    std::string download_path = output_path;
    std::string final_path;
    
    if (mp4_mode_) {
        // If we're in MP4 mode, download to .mkv first
        if (output_path.substr(output_path.length() - 4) == ".mp4") {
            download_path = output_path.substr(0, output_path.length() - 4) + ".mkv";
            final_path = output_path;
        }
    }

    std::cout << "Downloading to: " << download_path << std::endl;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    FILE* fp = fopen(download_path.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to open output file: " + download_path);
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    
    CURLcode res = curl_easy_perform(curl);
    
    fclose(fp);
    curl_easy_cleanup(curl);
    
    std::cout << std::endl;
    
    if (res != CURLE_OK) {
        fs::remove(download_path);
        throw std::runtime_error("Download failed: " + std::string(curl_easy_strerror(res)));
    }

    // Convert to MP4 if needed
    if (mp4_mode_ && !final_path.empty()) {
        std::cout << "Converting to MP4..." << std::endl;
        if (!convertToMp4(download_path, final_path)) {
            fs::remove(download_path);
            throw std::runtime_error("Failed to convert to MP4");
        }
        fs::remove(download_path); // Remove the temporary MKV file
    }
}

void Downloader::setProgressCallback(std::function<void(int, int)> callback) {
    progress_callback_ = std::move(callback);
}
