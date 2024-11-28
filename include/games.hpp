#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <stdexcept>

class GameError : public std::runtime_error {
public:
    explicit GameError(const std::string& message) : std::runtime_error(message) {}
};

class GameNetworkError : public GameError {
public:
    explicit GameNetworkError(const std::string& message) : GameError(message) {}
};

class GameNotFoundError : public GameError {
public:
    explicit GameNotFoundError(const std::string& message) : GameError(message) {}
};

class GameParseError : public GameError {
public:
    explicit GameParseError(const std::string& message) : GameError(message) {}
};

struct Game {
    std::string id;
    std::string title;
    std::string date;
    std::string last_updated;
    std::string direct_link;
};

class Games {
public:
    explicit Games(const std::string& api_key);
    
    std::vector<Game> search(const std::string& query);
    Game getGameDetails(const std::string& id);
    std::string getDirectLink(const std::string& id);

private:
    std::string api_key_;
    std::string makeRequest(const std::string& endpoint);
    void validateResponse(const nlohmann::json& json, const std::vector<std::string>& required_fields);
}; 