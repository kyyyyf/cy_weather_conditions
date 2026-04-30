#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct AqBand {
    double low{0}, moderate{0}, high{0};
};

struct LocationInfo {
    std::string name;
    std::string weather_code;
    std::string aq_station;
};

struct Config {
    std::filesystem::path database_path;
    uint16_t port{3000};
    std::vector<LocationInfo> locations;
    std::unordered_map<std::string, AqBand> aq_thresholds;

    [[nodiscard]]
    static std::expected<Config, std::string> load(std::string_view path);
};
