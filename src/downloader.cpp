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
    
    if (duration.count() >= 100) {
        // Get terminal width
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        int termWidth = w.ws_col;
        
        // Calculate sizes for components
        int progressBarWidth = std::min(50, termWidth - 40); // Shrink progress bar if needed
        int progress = static_cast<int>((dlnow * 100.0) / dltotal);
        
        // Calculate speed
        double speed = (dlnow - lastBytes) * 1000.0 / duration.count();
        double speedMB = speed / 1024.0 / 1024.0;
        
        // Calculate sizes
        double downloadedMB = dlnow / 1024.0 / 1024.0;
        double totalMB = dltotal / 1024.0 / 1024.0;
        
        // Format the output string first
        std::stringstream ss;
        ss << "\r[";
        for (int i = 0; i < progressBarWidth; i++) {
            if (i < (progress * progressBarWidth) / 100) {
                ss << "=";
            } else if (i == (progress * progressBarWidth) / 100) {
                ss << ">";
            } else {
                ss << " ";
            }
        }
        ss << "] " << std::fixed << std::setprecision(2) 
           << downloadedMB << "MB/" << totalMB << "MB @ " 
           << speedMB << "MB/s";
        
        std::string output = ss.str();
        
        // Truncate if longer than terminal width
        if (output.length() > static_cast<size_t>(termWidth)) {
            output = output.substr(0, termWidth - 3) + "...";
        }
        
        std::cout << "\033[2K" << output << std::flush;
        
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
    std::string command = "ffmpeg -i \"" + input_path + 
                         "\" -map 0:v -map 0:a -map 0:s? -map_metadata -1 " +
                         "-metadata title= -metadata description= -metadata comment= " +
                         "-metadata synopsis= -metadata show= -metadata episode_id= " +
                         "-metadata network= -metadata genre= " +
                         "-c copy \"" + output_path + "\" -y";
    return system(command.c_str()) == 0;
}

bool isM3U8Url(const std::string& url) {
    return url.find(".m3u8") != std::string::npos;
}

size_t ffmpegProgressCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    static auto lastUpdate = std::chrono::steady_clock::now();
    static int64_t lastDuration = 0;
    
    std::string line(static_cast<char*>(ptr), size * nmemb);
    
    if (line.find("time=") != std::string::npos) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate);
        
        if (duration.count() >= 100) {
            size_t timePos = line.find("time=") + 5;
            std::string timeStr = line.substr(timePos, 8);
            
            int hours = std::stoi(timeStr.substr(0, 2));
            int minutes = std::stoi(timeStr.substr(3, 2));
            int seconds = std::stoi(timeStr.substr(6, 2));
            int64_t currentDuration = hours * 3600 + minutes * 60 + seconds;
            
            double speed = (currentDuration - lastDuration) * 1000.0 / duration.count();
            
            std::cout << "\033[2K\r"; 
            std::cout << "Downloading [";
            
            int progress = (currentDuration * 50) / (2 * 3600);
            for (int i = 0; i < 50; i++) {
                if (i < progress) {
                    std::cout << "=";
                } else if (i == progress) {
                    std::cout << ">";
                } else {
                    std::cout << " ";
                }
            }
            
            std::cout << "] " << timeStr << " @ " << std::fixed << std::setprecision(2) << speed << "x";
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
        if (output_path.substr(output_path.length() - 4) == ".mp4") {
            download_path = output_path.substr(0, output_path.length() - 4) + ".mkv";
            final_path = output_path;
        }
    }

    std::cout << "\033[2K\rDownloading to: " << download_path << std::flush;
    
    if (isM3U8Url(url)) {
        std::string command = "ffmpeg -hide_banner -loglevel error -stats -i \"" + url + 
                            "\" -map 0:v -map 0:a -map 0:s? -map_metadata -1 " +
                            "-metadata title= -metadata description= -metadata comment= " +
                            "-metadata synopsis= -metadata show= -metadata episode_id= " +
                            "-metadata network= -metadata genre= " +
                            "-c copy \"" + download_path + "\" 2>&1";
        
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("Failed to start ffmpeg");
        }
        
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            ffmpegProgressCallback(buffer, 1, strlen(buffer), nullptr);
        }
        
        int status = pclose(pipe);
        std::cout << std::endl;
        
        if (status != 0) {
            fs::remove(download_path);
            throw std::runtime_error("Failed to download m3u8 stream");
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
        
        if (res == CURLE_OK) {
            std::string tempPath = download_path + ".processing";
            std::rename(download_path.c_str(), tempPath.c_str());
            
            std::string command = "ffmpeg -i \"" + tempPath + 
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

    if (mp4_mode_ && !final_path.empty() && !isM3U8Url(url)) {
        std::cout << "\033[2K\rConverting to MP4..." << std::flush;
        if (!convertToMp4(download_path, final_path)) {
            fs::remove(download_path);
            throw std::runtime_error("Failed to convert to MP4");
        }
        fs::remove(download_path);
    }
    
    std::cout << std::endl;
}

void Downloader::setProgressCallback(std::function<void(int, int)> callback) {
    progress_callback_ = std::move(callback);
}
