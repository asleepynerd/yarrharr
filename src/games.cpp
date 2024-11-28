#include "games.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

Games::Games(const std::string& api_key) : api_key_(api_key) {}

void Games::validateResponse(const nlohmann::json& json, const std::vector<std::string>& required_fields) {
    for (const auto& field : required_fields) {
        if (!json.contains(field)) {
            throw GameParseError("Missing required field: " + field);
        }
        if (json[field].is_null()) {
            throw GameParseError("Field is null: " + field);
        }
    }
}

std::string Games::makeRequest(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    std::string readBuffer;
    long http_code = 0;
    
    if (!curl) {
        throw GameNetworkError("Failed to initialize CURL");
    }
    
    std::string url = "https://sleepy.engineer/api/yarrharr/games" + endpoint;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    
    struct curl_slist* headers = NULL;
    if (!api_key_.empty()) {
        headers = curl_slist_append(headers, ("X-API-Key: " + api_key_).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    CURLcode res = curl_easy_perform(curl);
    
    if (headers) {
        curl_slist_free_all(headers);
    }
    
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        throw GameNetworkError(std::string("Network error: ") + curl_easy_strerror(res));
    }
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    
    if (http_code == 404) {
        throw GameNotFoundError("Game not found");
    }
    else if (http_code == 401) {
        throw GameNetworkError("Unauthorized: Invalid API key");
    }
    else if (http_code == 403) {
        throw GameNetworkError("Forbidden: Access denied");
    }
    else if (http_code != 200) {
        std::stringstream ss;
        ss << "Server returned HTTP code " << http_code;
        throw GameNetworkError(ss.str());
    }
    
    return readBuffer;
}

std::vector<Game> Games::search(const std::string& query) {
    if (query.empty()) {
        throw GameError("Search query cannot be empty");
    }
    
    std::string encoded_query;
    {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw GameNetworkError("Failed to initialize CURL for query encoding");
        }
        char* escaped = curl_easy_escape(curl, query.c_str(), query.length());
        if (!escaped) {
            curl_easy_cleanup(curl);
            throw GameError("Failed to encode search query");
        }
        encoded_query = escaped;
        curl_free(escaped);
        curl_easy_cleanup(curl);
    }
    
    std::string response = makeRequest("?query=" + encoded_query);
    
    try {
        auto json = nlohmann::json::parse(response);
        if (!json.is_array()) {
            throw GameParseError("Expected array response for search");
        }
        
        std::vector<Game> games;
        for (const auto& item : json) {
            validateResponse(item, {"id", "title", "date", "last_updated"});
            
            Game game;
            game.id = std::to_string(item["id"].get<int>());
            game.title = item["title"].get<std::string>();
            game.date = item["date"].get<std::string>();
            game.last_updated = item["last_updated"].get<std::string>();
            games.push_back(game);
        }
        
        return games;
    }
    catch (const nlohmann::json::exception& e) {
        throw GameParseError(std::string("Failed to parse search response: ") + e.what());
    }
}

Game Games::getGameDetails(const std::string& id) {
    if (id.empty()) {
        throw GameError("Game ID cannot be empty");
    }
    
    std::string response = makeRequest("/" + id);
    
    try {
        auto json = nlohmann::json::parse(response);
        validateResponse(json, {"id", "title", "date", "last_updated", "direct_link"});
        
        Game game;
        game.id = std::to_string(json["id"].get<int>());
        game.title = json["title"].get<std::string>();
        game.date = json["date"].get<std::string>();
        game.last_updated = json["last_updated"].get<std::string>();
        game.direct_link = json["direct_link"].get<std::string>();
        
        return game;
    }
    catch (const nlohmann::json::exception& e) {
        throw GameParseError(std::string("Failed to parse game details: ") + e.what());
    }
}

std::string Games::getDirectLink(const std::string& id) {
    if (id.empty()) {
        throw GameError("Game ID cannot be empty");
    }
    return makeRequest("/" + id + "/direct");
} 