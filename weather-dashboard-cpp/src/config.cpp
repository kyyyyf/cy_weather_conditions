#include "config.hpp"
#include <toml++/toml.hpp>
#include <format>

std::expected<Config, std::string> Config::load(std::string_view path) {
    toml::table tbl;
    try {
        tbl = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        return std::unexpected(std::format("Failed to parse {}: {}", path, e.description()));
    }

    Config cfg;

    auto db = tbl["database_path"].value<std::string>();
    if (!db) return std::unexpected("Missing required key: database_path");
    cfg.database_path = *db;

    if (auto p = tbl["port"].value<uint16_t>())
        cfg.port = *p;

    // [[locations]]
    if (auto* locs = tbl["locations"].as_array()) {
        for (auto& elem : *locs) {
            auto* t = elem.as_table();
            if (!t) continue;
            LocationInfo loc;
            loc.name         = (*t)["name"].value_or(std::string{});
            loc.weather_code = (*t)["weather_code"].value_or(std::string{});
            loc.aq_station   = (*t)["aq_station"].value_or(std::string{});
            if (!loc.name.empty())
                cfg.locations.push_back(std::move(loc));
        }
    }

    // [aq_thresholds.PM10], [aq_thresholds."PM2.5"], …
    if (auto* thr = tbl["aq_thresholds"].as_table()) {
        for (auto& [key, val] : *thr) {
            auto* bt = val.as_table();
            if (!bt) continue;
            AqBand band;
            band.low      = (*bt)["low"].value_or(0.0);
            band.moderate = (*bt)["moderate"].value_or(0.0);
            band.high     = (*bt)["high"].value_or(0.0);
            cfg.aq_thresholds[std::string{key}] = band;
        }
    }

    return cfg;
}
