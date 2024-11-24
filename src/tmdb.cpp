#include "tmdb.hpp"
#include "utils.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iostream>


static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

TMDB::TMDB(const std::string& api_key) : api_key_(api_key) {}

std::string TMDB::makeRequest(const std::string& endpoint) {
    
    CURL* curl = curl_easy_init();
    std::string readBuffer;
    
    if (curl) {
        std::string url = "https://api.themoviedb.org/3" + endpoint + 
                         (endpoint.find('?') != std::string::npos ? "&" : "?") +
                         "api_key=" + api_key_;
        
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cout << "CURL error: " << curl_easy_strerror(res) << std::endl;
        }
        
        curl_easy_cleanup(curl);
    }
    
    return readBuffer;
}

std::vector<nlohmann::json> TMDB::search(const std::string& query) {
    std::string encoded_query;
    {
        CURL* curl = curl_easy_init();
        char* escaped = curl_easy_escape(curl, query.c_str(), query.length());
        encoded_query = escaped;
        curl_free(escaped);
        curl_easy_cleanup(curl);
    }
    
    std::string response = makeRequest("/search/multi?query=" + encoded_query);
    auto json = nlohmann::json::parse(response);
    return json["results"].get<std::vector<nlohmann::json>>();
}

Show TMDB::getShowDetails(const std::string& id) {    
    std::string response = makeRequest("/tv/" + id);
    
    auto json = nlohmann::json::parse(response);
    
    Show show;
    show.id = id;
    
    if (json.contains("name") && !json["name"].is_null()) {
        show.name = json["name"].get<std::string>();
    } else {
        show.name = "Unknown";
    }
    
    if (json.contains("first_air_date") && !json["first_air_date"].is_null()) {
        show.first_air_date = json["first_air_date"].get<std::string>();
    } else {
        show.first_air_date = "Unknown";
    }
    
    if (json.contains("seasons") && !json["seasons"].is_null()) {
        for (const auto& season : json["seasons"]) {
            if (!season.is_null() && season.contains("season_number")) {
                auto season_num = season["season_number"].get<int>();
                auto season_episodes = getSeasonEpisodes(id, season_num);
                show.episodes.insert(show.episodes.end(), 
                                   season_episodes.begin(), 
                                   season_episodes.end());
            }
        }
    } else {
        std::cout << "No seasons data available" << std::endl;
    }
    
    return show;
}

Movie TMDB::getMovieDetails(const std::string& id) {
    std::string response = makeRequest("/movie/" + id);
    auto json = nlohmann::json::parse(response);
    
    Movie movie;
    movie.id = id;
    movie.title = json["title"].get<std::string>();
    movie.release_date = json["release_date"].get<std::string>();
    
    return movie;
}

std::vector<Episode> TMDB::getSeasonEpisodes(const std::string& show_id, int season) {
    std::string response = makeRequest("/tv/" + show_id + "/season/" + std::to_string(season));
    auto json = nlohmann::json::parse(response);
    
    std::vector<Episode> episodes;
    if (json.contains("episodes") && !json["episodes"].is_null()) {
        for (const auto& ep : json["episodes"]) {
            if (!ep.is_null()) {
                Episode episode;
                episode.season = season; 
                episode.episode = ep["episode_number"].get<int>();
                episode.name = ep.contains("name") && !ep["name"].is_null() 
                    ? ep["name"].get<std::string>() 
                    : "Episode " + std::to_string(episode.episode);
                episode.air_date = ep.contains("air_date") && !ep["air_date"].is_null()
                    ? ep["air_date"].get<std::string>()
                    : "Unknown";
                episodes.push_back(episode);
            }
        }
    }
    
    return episodes;
}