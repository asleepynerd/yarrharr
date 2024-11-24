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
        // Clear the current line
        std::cout << "\033[2K\r";  // ANSI escape code to clear line and return carriage
        
        // Calculate progress
        int progress = static_cast<int>((dlnow * 100.0) / dltotal);
        
        // Calculate current speed
        double speed = (dlnow - lastBytes) * 1000.0 / duration.count(); // bytes per second
        double speedMB = speed / 1024.0 / 1024.0; // MB per second
        
        // Draw progress bar
        std::cout << "Downloading [";
        for (int i = 0; i < 50; i++) {
            if (i < progress / 2) {
                std::cout << "=";
            } else if (i == progress / 2) {
                std::cout << ">";
            } else {
                std::cout << " ";
            }
        }
        
        // Show downloaded/total size and speed
        double downloadedMB = dlnow / 1024.0 / 1024.0;
        double totalMB = dltotal / 1024.0 / 1024.0;
        std::cout << "] " << std::fixed << std::setprecision(2) 
                 << downloadedMB << "MB/" << totalMB << "MB"
                 << " @ " << speedMB << " MB/s";
        
        std::cout.flush();  // Make sure to flush the output
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

Downloader::Downloader(const std::string& base_url, bool mp4_mode, bool skip_specials) 
    : base_url_(base_url), mp4_mode_(mp4_mode), skip_specials_(skip_specials) {
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
    if (skip_specials_ && season == 0) {
        return;  // Skip season 0 if skip_specials_ is true
    }
    
    for (const auto& episode : show.episodes) {
        if (episode.season == season) {
            downloadEpisode(show, episode, output_dir);
        }
    }
}

void Downloader::downloadShow(const Show& show, const std::string& output_dir) {
    std::set<int, std::less<int>> seasons;
    for (const auto& episode : show.episodes) {
        if (!skip_specials_ || episode.season > 0) {  // Skip season 0 if skip_specials_ is true
            seasons.insert(episode.season);
        }
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
        if (output_path.substr(output_path.length() - 4) == ".mp4") {
            download_path = output_path.substr(0, output_path.length() - 4) + ".mkv";
            final_path = output_path;
        }
    }

    // Clear line and print the "Downloading to:" message
    std::cout << "\033[2K\rDownloading to: " << download_path << std::flush;
    
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
    
    // Add API key header
    struct curl_slist* headers = NULL;
    if (!api_key_.empty()) {
        headers = curl_slist_append(headers, ("X-API-Key: " + api_key_).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    CURLcode res = curl_easy_perform(curl);
    
    if (headers) {
        curl_slist_free_all(headers);
    }
    
    fclose(fp);
    curl_easy_cleanup(curl);
    
    std::cout << std::endl;
    
    if (res != CURLE_OK) {
        fs::remove(download_path);
        throw std::runtime_error("Download failed: " + std::string(curl_easy_strerror(res)));
    }

    if (mp4_mode_ && !final_path.empty()) {
        // Clear line and print the conversion message
        std::cout << "\033[2K\rConverting to MP4..." << std::flush;
        if (!convertToMp4(download_path, final_path)) {
            fs::remove(download_path);
            throw std::runtime_error("Failed to convert to MP4");
        }
        fs::remove(download_path);
    }
    
    // Add a newline at the very end
    std::cout << std::endl;
}

void Downloader::setProgressCallback(std::function<void(int, int)> callback) {
    progress_callback_ = std::move(callback);
}
