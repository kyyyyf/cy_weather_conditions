#include "config.hpp"
#include "database.hpp"
#include "logger.hpp"
#include "dom_collector.hpp"
#include "air_quality_collector.hpp"
#include <algorithm>    // std::ranges::contains
#include <format>
#include <print>
#include <ranges>
#include <span>

// ── collect_weather ───────────────────────────────────────────────────────────

static void collect_weather(
    Database&                     db,
    std::span<const std::string>  codes,
    const Logger&                 log)
{
    log.info(std::format("weather: fetching {} station(s)", codes.size()));

    const auto result = fetch_weather(codes);
    if (!result) {
        log.error(std::format("weather fetch failed: {}", result.error()));
        return;
    }

    for (const auto& s : *result) {
        std::size_t stored = 0, failed = 0;
        for (const auto& obs : s.observations) {
            const auto r = db.insert_weather(
                {s.station_code, s.timestamp, obs.name, obs.value, obs.unit});
            if (r) { ++stored; }
            else {
                log.error(std::format("weather DB insert ({}/{}): {}",
                    s.station_code, obs.name, r.error()));
                ++failed;
            }
        }
        if (failed == 0)
            log.info(std::format("weather OK: {} @ {} – {} stored",
                s.station_code, s.timestamp, stored));
        else
            log.warn(std::format("weather PARTIAL: {} @ {} – {} stored, {} failed",
                s.station_code, s.timestamp, stored, failed));
    }

    // Warn about any configured code absent from the feed
    for (const auto& code : codes) {
        if (!std::ranges::contains(*result, code, &StationData::station_code))
            log.warn(std::format("weather: station '{}' not found in XML", code));
    }
}

// ── collect_air_quality ───────────────────────────────────────────────────────

static void collect_air_quality(
    Database&                     db,
    std::span<const std::string>  names,
    const Logger&                 log)
{
    log.info(std::format("air_quality: fetching {} station(s)", names.size()));

    const auto result = fetch_air_quality(names);
    if (!result) {
        log.error(std::format("air_quality fetch failed: {}", result.error()));
        return;
    }

    for (const auto& rdg : *result) {
        std::size_t stored = 0, failed = 0;
        for (const auto& m : rdg.measurements) {
            const auto r = db.insert_air_quality(
                {rdg.station_name, rdg.timestamp, m.pollutant, m.value, m.unit});
            if (r) { ++stored; }
            else {
                log.error(std::format("air_quality DB insert ({}/{}): {}",
                    rdg.station_name, m.pollutant, r.error()));
                ++failed;
            }
        }
        if (failed == 0)
            log.info(std::format("air_quality OK: {} @ {} – {} stored",
                rdg.station_name, rdg.timestamp, stored));
        else
            log.warn(std::format("air_quality PARTIAL: {} @ {} – {} stored, {} failed",
                rdg.station_name, rdg.timestamp, stored, failed));
    }

    for (const auto& name : names) {
        if (!std::ranges::contains(*result, name, &StationReading::station_name))
            log.warn(std::format("air_quality: station '{}' not found in response", name));
    }
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    const std::string cfg_path = (argc > 1) ? argv[1] : "config.toml";

    auto cfg = Config::load(cfg_path);
    if (!cfg) { std::println(stderr, "Error: {}", cfg.error()); return 1; }

    auto db = Database::open(cfg->database_path);
    if (!db)  { std::println(stderr, "Error: {}", db.error());  return 1; }

    Logger log{cfg->log_file};
    log.info("collection started");
    collect_weather    (*db, cfg->weather_codes, log);
    collect_air_quality(*db, cfg->aq_names,      log);
    log.info("collection complete");
    return 0;
}
