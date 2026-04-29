#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

struct Config {
    std::filesystem::path database_path;
    std::filesystem::path log_file{"collector.log"};
    std::vector<std::string> weather_codes;
    std::vector<std::string> aq_names;

    [[nodiscard]]
    static std::expected<Config, std::string> load(std::string_view path);
};
