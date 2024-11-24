#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct Episode {
    int season;
    int episode;
    std::string name;
    std::string air_date;
};

struct Show {
    std::string id;
    std::string name;
    std::string first_air_date;
    std::vector<Episode> episodes;
};

struct Movie {
    std::string id;
    std::string title;
    std::string release_date;
};

class TMDB {
public:
    explicit TMDB(const std::string& api_key);
    
    std::vector<nlohmann::json> search(const std::string& query);
    Show getShowDetails(const std::string& id);
    Movie getMovieDetails(const std::string& id);
    std::vector<Episode> getSeasonEpisodes(const std::string& show_id, int season);

private:
    std::string api_key_;
    std::string makeRequest(const std::string& endpoint);
};