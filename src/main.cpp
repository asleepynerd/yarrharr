#include <iostream>
#include <string>
#include <filesystem>
#include "config.hpp"
#include "tmdb.hpp"
#include "downloader.hpp"
#include "utils.hpp"
#include <iomanip>
#include <map>
#include <sys/ioctl.h>
#include <unistd.h>
#include "version.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <curl/curl.h>
#include <sstream>
#include "download_utils.hpp"

namespace fs = std::filesystem;

void printHelp() {
    std::cout << "Usage: yarrharr <command> [options]\n\n"
              << "Commands:\n"
              << "  search <query>           Search for movies and TV shows\n"
              << "  show <id>               Show details about a TV show\n"
              << "  movie <id>              Show details about a movie\n"
              << "  download [options]       Download content\n"
              << "    --movie <id>          Download a movie\n"
              << "    --show <id>           Download entire show\n"
              << "    --season <num>        Download specific season\n"
              << "    --episode <num>       Download specific episode\n"
              << "    --skip-specials       Skip downloading season 0 (specials)\n"
              << "  config                  Configure API keys and settings\n"
              << "  help                    Show this help message\n"
              << "  Global options:\n"
              << "    --mp4                Convert downloads to MP4 format (requires ffmpeg)\n";
}

bool updateBinary(const char* execPath) {
    try {
        std::string latestVersion = version::getLatestVersion();
        if (!version::isVersionNewer(latestVersion, std::string(version::CURRENT_VERSION))) {
            std::cout << "You are already running the latest version (" << version::CURRENT_VERSION << ").\n";
            return true;
        }

        #if defined(__APPLE__)
            #if defined(__arm64__)
                const std::string platform = "macos-arm64";
            #else
                const std::string platform = "macos-x86_64";
            #endif
        #elif defined(__linux__)
            #if defined(__aarch64__)
                const std::string platform = "linux-aarch64";
            #else
                const std::string platform = "linux-x86_64";
            #endif
        #else
            std::cerr << "Unsupported platform for auto-update\n";
            return false;
        #endif

        fs::path tempFile = fs::temp_directory_path() / "yarrharr.new";
        
        std::cout << "Downloading update " << version::CURRENT_VERSION << " → " << latestVersion << "...\n";
        
        std::string downloadUrl = "https://github.com/asleepynerd/yarrharr/releases/download/v" + 
                                latestVersion + "/yarrharr-" + platform;

        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize download");
        }

        FILE* fp = fopen(tempFile.string().c_str(), "wb");
        if (!fp) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("Failed to create temporary file");
        }

        curl_easy_setopt(curl, CURLOPT_URL, downloadUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        CURLcode res = curl_easy_perform(curl);
        fclose(fp);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            fs::remove(tempFile);
            throw std::runtime_error("Failed to download update");
        }

        std::cout << std::endl;

        fs::permissions(tempFile, 
            fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write,
            fs::perm_options::add);

        fs::path currentExe = fs::canonical(fs::path(execPath));

        fs::rename(tempFile, currentExe);

        std::cout << "Update complete! Please restart yarrharr.\n";
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Update failed: " << e.what() << "\n";
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        version::checkForUpdates();
        printHelp();
        return 1;
    }

    if (argc >= 2 && std::string(argv[1]) != "help") {
        version::checkForUpdates();
    }

    try {
        auto config = Config::load("config.json");
        std::string command = argv[1];

        if (command == "config") {
            std::string tmdb_api_key;
            std::string yarrharr_api_key;
            
            std::cout << "Enter TMDB API key: ";
            std::getline(std::cin, tmdb_api_key);
            
            std::cout << "Enter YarrHarr API key (email api@sleepy.engineer to get one): ";
            std::getline(std::cin, yarrharr_api_key);
            
            Config newConfig{
                tmdb_api_key,
                yarrharr_api_key,
                config.download_path,
                true,
                true
            };
            newConfig.save("config.json");
            return 0;
        }

        if (config.tmdb_api_key.empty()) {
            std::cerr << "Error: No TMDB API key found. Please run 'yarrharr config' to set it up.\n";
            return 1;
        }

        TMDB tmdb(config.tmdb_api_key);
        bool mp4_mode = false;
        bool skip_specials = false;
        std::string id;
        int season = -1;
        int episode = -1;
        bool isMovie = false;

        for (int i = 2; i < argc; i++) {
            std::string option = argv[i];
            if (option == "--mp4") {
                mp4_mode = true;
                continue;
            }
            if (option == "--skip-specials") {
                skip_specials = true;
                continue;
            }
            if (i + 1 >= argc) break;
            
            if (option == "--movie") {
                id = argv[i + 1];
                isMovie = true;
            }
            else if (option == "--show") {
                id = argv[i + 1];
            }
            else if (option == "--season") {
                season = std::stoi(argv[i + 1]);
            }
            else if (option == "--episode") {
                episode = std::stoi(argv[i + 1]);
            }
        }

        if (command == "download") {
            Downloader downloader("https://sleepy.engineer/api/yarrharr/direct", mp4_mode, skip_specials);
            
            if (!config.yarrharr_api_key.empty()) {
                downloader.setApiKey(config.yarrharr_api_key);
            } else {
                std::cerr << "Error: No YarrHarr API key found. Please run 'yarrharr config' to set it up.\n";
                return 1;
            }

            if (isMovie) {
                auto movie = tmdb.getMovieDetails(id);
                downloader.downloadMovie(movie, config.download_path);
            }
            else if (!id.empty()) {
                auto show = tmdb.getShowDetails(id);
                if (season >= 0 && episode >= 0) {
                    for (const auto& ep : show.episodes) {
                        if (ep.season == season && ep.episode == episode) {
                            downloader.downloadEpisode(show, ep, config.download_path);
                            break;
                        }
                    }
                }
                else if (season >= 0) {
                    downloader.downloadSeason(show, season, config.download_path);
                }
                else {
                    downloader.downloadShow(show, config.download_path);
                }
            }
        }
        else if (command == "search" && argc > 2) {
            std::string query;
            for (int i = 2; i < argc; i++) {
                query += std::string(argv[i]) + " ";
            }

            auto results = tmdb.search(query);
            for (size_t i = 0; i < results.size(); i++) {
                const auto& result = results[i];
                std::cout << i + 1 << ". ";
                if (result["media_type"] == "movie") {
                    std::cout << "[Movie] " << result["title"].get<std::string>() << " (" << result["release_date"].get<std::string>().substr(0, 4) << ")" << " - " << result["id"].get<std::int64_t>() << "\n";
                } else {
                    std::cout << "[TV] " << result["name"].get<std::string>() << " - " << result["id"].get<std::int64_t>() << "\n";
                }
            }
        }
        else if (command == "show" && argc > 2) {
            auto show = tmdb.getShowDetails(argv[2]);
            std::cout << "Show: " << show.name << "\n"
                     << "First aired: " << show.first_air_date << "\n\n";

            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            int termWidth = w.ws_col;

            std::map<int, std::vector<Episode>> seasons;
            for (const auto& ep : show.episodes) {
                seasons[ep.season].push_back(ep);
            }

            int numSeasons = seasons.size();
            int colWidth = (termWidth / numSeasons) - 3;
            int numWidth = 3; 

            for (const auto& [season, _] : seasons) {
                std::cout << std::setw(colWidth) << std::left 
                         << ("Season " + std::to_string(season));
                if (season != seasons.rbegin()->first) 
                    std::cout << " | ";
            }
            std::cout << "\n";

            for (size_t i = 0; i < seasons.size(); i++) {
                std::cout << std::string(colWidth, '-');
                if (i < seasons.size() - 1) 
                    std::cout << "-+-";
            }
            std::cout << "\n";

            size_t maxEpisodes = 0;
            for (const auto& [_, episodes] : seasons) {
                maxEpisodes = std::max(maxEpisodes, episodes.size());
            }

            for (size_t epIndex = 0; epIndex < maxEpisodes; epIndex++) {
                std::vector<std::vector<std::string>> wrappedLines(seasons.size());
                int maxWrappedLines = 1;

                int colIndex = 0;
                for (const auto& [season, episodes] : seasons) {
                    if (epIndex < episodes.size()) {
                        const auto& ep = episodes[epIndex];
                        std::string epText = utils::padNumber(ep.episode, 2) + ". " + ep.name;
                        wrappedLines[colIndex] = utils::wrapText(epText, colWidth - 1);
                        maxWrappedLines = std::max(maxWrappedLines, (int)wrappedLines[colIndex].size());
                    }
                    colIndex++;
                }

                for (int line = 0; line < maxWrappedLines; line++) {
                    colIndex = 0;
                    for (const auto& [season, episodes] : seasons) {
                        if (line < wrappedLines[colIndex].size()) {
                            std::cout << std::setw(colWidth) << std::left << wrappedLines[colIndex][line];
                        } else {
                            std::cout << std::setw(colWidth) << " ";
                        }
                        if (colIndex < numSeasons - 1) std::cout << " | ";
                        colIndex++;
                    }
                    std::cout << "\n";
                }
            }
        }
        else if (command == "movie" && argc > 2) {
            auto movie = tmdb.getMovieDetails(argv[2]);
            std::cout << "\nMovie: " << movie.title << "\n"
                      << "Release date: " << movie.release_date << "\n"
                      << "TMDB ID: " << movie.id << "\n\n";
              
            std::cout << "To download this movie:\n"
                      << "./yarrharr download --movie " << movie.id << "\n";
        }
        else if (command == "update") {
            if (updateBinary(argv[0])) {
                return 0;
            }
            return 1;
        }
        else {
            printHelp();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}