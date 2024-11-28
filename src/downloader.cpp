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
#include <sys/ioctl.h>
#include <unistd.h>
#include "download_utils.hpp"
#include <algorithm>  // for std::transform

namespace fs = std::filesystem;

std::string formatTime(double seconds);

bool isFFmpegAvailable() {
    #ifdef _WIN32
    return system("where ffmpeg >nul 2>nul") == 0;
    #else
    return system("which ffmpeg >/dev/null 2>&1") == 0;
    #endif
}

bool convertToMp4(const std::string& input_path, const std::string& output_path) {
    std::string command = "ffmpeg -v quiet -stats_period 0.1 -i \"" + input_path + 
                         "\" -map 0:v -map 0:a -map 0:s? -map_metadata -1 " +
                         "-metadata title= -metadata description= -metadata comment= " +
                         "-metadata synopsis= -metadata show= -metadata episode_id= " +
                         "-metadata network= -metadata genre= " +
                         "-c copy \"" + output_path + "\" -y";
    return system(command.c_str()) == 0;
}

size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    std::string* contentType = static_cast<std::string*>(userdata);
    std::string header(buffer, size * nitems);
    
    std::string headerLower = header;
    std::transform(headerLower.begin(), headerLower.end(), headerLower.begin(), ::tolower);
    
    if (headerLower.find("content-type:") == 0) {
        *contentType = header.substr(header.find(":") + 1);
        contentType->erase(0, contentType->find_first_not_of(" \n\r\t"));
        contentType->erase(contentType->find_last_not_of(" \n\r\t") + 1);
    }
    return size * nitems;
}

bool isM3U8Url(const std::string& url, const std::string& api_key = "") {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    std::string contentType;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &contentType);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    struct curl_slist* headers = NULL;
    if (!api_key.empty()) {
        headers = curl_slist_append(headers, ("X-API-Key: " + api_key).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return false;
    }
    
    return contentType == "application/vnd.apple.mpegurl" || 
           contentType == "application/x-mpegurl";
}

size_t ffmpegProgressCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    static auto lastUpdate = std::chrono::steady_clock::now();
    static int64_t lastDuration = 0;
    static bool initialized = false;
    static struct winsize w;
    
    if (!initialized) {
        std::cout.setf(std::ios::unitbuf);  // Disable buffering
        setvbuf(stdout, nullptr, _IONBF, 0);  // Disable stdio buffering
        initialized = true;
    }
    
    std::string line(static_cast<char*>(ptr), size * nmemb);
    
    if (line.find("time=") != std::string::npos) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate);
        
        if (duration.count() >= 100) {  // Update every 100ms
            // Get terminal width
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            int termWidth = w.ws_col;
            
            size_t timePos = line.find("time=") + 5;
            std::string timeStr = line.substr(timePos, 8);
            
            int hours = std::stoi(timeStr.substr(0, 2));
            int minutes = std::stoi(timeStr.substr(3, 2));
            int seconds = std::stoi(timeStr.substr(6, 2));
            int64_t currentDuration = hours * 3600 + minutes * 60 + seconds;
            
            double speed = (currentDuration - lastDuration) * 1000.0 / duration.count();
            
            // Calculate available width for progress bar
            int textWidth = 40;  // Space for text elements
            int barWidth = termWidth - textWidth;
            if (barWidth < 10) barWidth = 10;  // Minimum bar width
            
            std::cout << "\033[2K\r🎞  Downloading | [";
            
            int progress = (currentDuration * barWidth) / (2 * 3600);  // Assuming 2-hour max duration
            for (int i = 0; i < barWidth; i++) {
                if (i < progress) {
                    std::cout << "█";  // Full block
                } else if (i == progress) {
                    std::cout << "▓";  // Partial block
                } else {
                    std::cout << "░";  // Empty block
                }
            }
            
            int percentage = (currentDuration * 100) / (2 * 3600);
            std::cout << "] " << std::setw(3) << percentage << "% | " 
                     << timeStr << " @ " << std::fixed << std::setprecision(2) << speed << "x";
            std::cout.flush();
            
            lastUpdate = now;
            lastDuration = currentDuration;
        }
    }
    
    return size * nmemb;
}

Downloader::Downloader(const std::string& base_url, bool mp4_mode, bool skip_specials) 
    : base_url_(base_url), mp4_mode_(mp4_mode), skip_specials_(skip_specials) {
    if (mp4_mode_ && !isFFmpegAvailable()) {
        throw std::runtime_error("FFmpeg not found in PATH. Required for MP4 conversion.");
    }
}

void Downloader::downloadMovie(const Movie& movie, const std::string& output_dir) {
    // Build URL first
    std::string url = buildUrl(movie.id);
    
    // Now we can use the URL to determine the extension
    std::string filename = utils::sanitizeFilename(
        movie.title + " (" + movie.release_date.substr(0, 4) + ")" + 
        (isM3U8Url(url, api_key_) ? ".mp4" : ".mkv")
    );
    
    std::string output_path = (fs::path(output_dir) / "Movies" / filename).string();
    utils::createDirectoryIfNotExists((fs::path(output_dir) / "Movies").string());
    
    downloadFile(url, output_path);
}

void Downloader::downloadEpisode(const Show& show, const Episode& episode, const std::string& output_dir) {
    std::string show_dir = (fs::path(output_dir) / "TV Shows" / utils::sanitizeFilename(show.name)).string();
    
    std::string season_dir = (fs::path(show_dir) / 
        (std::string("Season ") + (episode.season < 10 ? "0" : "") + std::to_string(episode.season))).string();
    
    utils::createDirectoryIfNotExists(show_dir);
    utils::createDirectoryIfNotExists(season_dir);
    
    std::string url = buildUrl(show.id, episode.season, episode.episode);
    
    std::string filename = utils::sanitizeFilename(
        show.name + " - S" + 
        (episode.season < 10 ? "0" : "") + std::to_string(episode.season) + "E" + 
        (episode.episode < 10 ? "0" : "") + std::to_string(episode.episode) + " - " + 
        (isM3U8Url(url, api_key_) ? ".mp4" : ".mkv")
    );
    
    std::string output_path = (fs::path(season_dir) / filename).string();
    downloadFile(url, output_path);
}

void Downloader::downloadSeason(const Show& show, int season, const std::string& output_dir) {
    if (skip_specials_ && season == 0) {
        return; 
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
        if (!skip_specials_ || episode.season > 0) { 
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
        if (output_path.substr(output_path.length() - 4) == ".mkv") {
            final_path = output_path.substr(0, output_path.length() - 4) + ".mp4";
            download_path = output_path;
        }
    }

    std::cout << "Downloading to: " << (final_path.empty() ? download_path : final_path) << std::endl;
    std::cout << std::endl;
    
    if (isM3U8Url(url, api_key_)) {
        std::string headers = !api_key_.empty() ? " -headers \"X-API-Key: " + api_key_ + "\"" : "";
        std::string command = "ffmpeg -nostats -hide_banner -loglevel error " + headers +
                              " -i \"" + url + "\" -map 0:v -map 0:a -map 0:s? -map_metadata -1 " +
                              "-metadata title= -metadata description= -metadata comment= " +
                              "-metadata synopsis= -metadata show= -metadata episode_id= " +
                              "-metadata network= -metadata genre= " +
                              "-c copy \"" + download_path + "\" -y " +
                              "-progress pipe:1 2>/dev/null";

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("Failed to start ffmpeg");
        }

        std::string line;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            line = buffer;
            parseProgress(line);
        }

        int status = pclose(pipe);
        if (status != 0) {
            fs::remove(download_path);
            throw std::runtime_error("FFmpeg exited with an error.");
        }
    } else {
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

        if (output_path.find(".mkv") != std::string::npos || 
            output_path.find(".mp4") != std::string::npos) {
            
            std::string tempPath = download_path + ".processing";
            std::rename(download_path.c_str(), tempPath.c_str());
            
            std::string command = "ffmpeg -stats_period 0.1 -i \"" + tempPath + 
                                "\" -map 0:v -map 0:a -map 0:s? -map_metadata -1 " +
                                "-metadata title= -metadata description= -metadata comment= " +
                                "-metadata synopsis= -metadata show= -metadata episode_id= " +
                                "-metadata network= -metadata genre= " +
                                "-c copy \"" + download_path + "\" -y";
            
            if (system(command.c_str()) != 0) {
                fs::remove(tempPath);
                throw std::runtime_error("Failed to strip metadata");
            }
            fs::remove(tempPath);
        }
    }

    if (mp4_mode_ && !final_path.empty()) {
        std::cout << "\033[2K\rConverting to MP4..." << std::flush;
        if (!convertToMp4(download_path, final_path)) {
            fs::remove(download_path);
            throw std::runtime_error("Failed to convert to MP4");
        }
        fs::remove(download_path);
    }
    
    std::cout << std::endl;
}

void Downloader::parseProgress(const std::string& line) {
    static bool first_update = true;
    static int64_t total_duration_ms = 0;
    static int64_t last_time_ms = 0;
    static auto lastUpdate = std::chrono::steady_clock::now();

    std::string key, value;
    size_t pos = line.find('=');
    if (pos != std::string::npos) {
        key = line.substr(0, pos);
        value = line.substr(pos + 1);
    } else {
        return;
    }

    if (key == "out_time_ms") {
        int64_t current_time_ms = std::stoll(value);

        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate);

        if (duration.count() >= 100) {  // Update every 100ms
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            int termWidth = w.ws_col;

            int textWidth = 40;
            int barWidth = termWidth - textWidth;
            if (barWidth < 10) barWidth = 10;

            double seconds = current_time_ms / 1000000.0;
            double progress = std::min(1.0, seconds / (3.5 * 3600.0));  // 3.5 hours max duration
            int filled = static_cast<int>(progress * barWidth);

            std::cout << "\033[A\033[2K\r🎞  Downloading | [";
            
            for (int i = 0; i < barWidth; ++i) {
                if (i < filled) {
                    std::cout << "█";
                } else if (i == filled) {
                    std::cout << "▓";
                } else {
                    std::cout << "░";
                }
            }

            int percentage = static_cast<int>(progress * 100);
            double speed = (current_time_ms - last_time_ms) / (duration.count() * 10.0);

            std::cout << "] " << std::setw(3) << percentage << "% | "
                     << formatTime(seconds) << " @ "
                     << std::fixed << std::setprecision(2) << speed << "x"
                     << std::flush;

            lastUpdate = now;
            last_time_ms = current_time_ms;
        }
    } else if (key == "progress" && value == "end") {
        // Move cursor up one line and clear it for the final message
        std::cout << "\033[A\033[2K\rDownload complete." << std::endl;
    }
}

void Downloader::setProgressCallback(std::function<void(int, int)> callback) {
    progress_callback_ = std::move(callback);
}

std::string formatTime(double seconds) {
    int hours = static_cast<int>(seconds) / 3600;
    int minutes = (static_cast<int>(seconds) % 3600) / 60;
    int secs = static_cast<int>(seconds) % 60;
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << minutes << ":"
       << std::setfill('0') << std::setw(2) << secs;
    return ss.str();
}
