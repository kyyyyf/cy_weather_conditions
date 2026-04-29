#include "config.hpp"
#include <toml++/toml.hpp>
#include <format>
#include <fstream>

std::expected<Config, std::string> Config::load(std::string_view path) {
    toml::table tbl;
    try {
        tbl = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        return std::unexpected(std::format("Failed to parse {}: {}", path, e.description()));
    }

    Config cfg;

    // database_path — required
    auto db = tbl["database_path"].value<std::string>();
    if (!db) {
        return std::unexpected("Missing required key: database_path");
    }
    cfg.database_path = *db;

    // log_file — optional, default "collector.log"
    if (auto lf = tbl["log_file"].value<std::string>()) {
        cfg.log_file = *lf;
    }

    // [weather_stations].codes — required
    auto* ws = tbl["weather_stations"].as_table();
    if (!ws) {
        return std::unexpected("Missing [weather_stations] section");
    }
    auto* codes = (*ws)["codes"].as_array();
    if (!codes) {
        return std::unexpected("Missing [weather_stations].codes");
    }
    for (auto& elem : *codes) {
        if (auto s = elem.value<std::string>()) {
            cfg.weather_codes.push_back(std::move(*s));
        }
    }

    // [air_quality_stations].names — required
    auto* aqs = tbl["air_quality_stations"].as_table();
    if (!aqs) {
        return std::unexpected("Missing [air_quality_stations] section");
    }
    auto* names = (*aqs)["names"].as_array();
    if (!names) {
        return std::unexpected("Missing [air_quality_stations].names");
    }
    for (auto& elem : *names) {
        if (auto s = elem.value<std::string>()) {
            cfg.aq_names.push_back(std::move(*s));
        }
    }

    return cfg;
}
