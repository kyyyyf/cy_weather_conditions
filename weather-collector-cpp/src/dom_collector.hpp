#pragma once
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct Observation {
    std::string          name;
    std::optional<double> value;  // null when station reports N/A
    std::string          unit;
};

struct StationData {
    std::string           station_code;
    std::string           timestamp;   // "YYYY-MM-DD HH:MM"
    std::vector<Observation> observations;
};

/// Fetches the DOM XML feed and returns data for the requested station codes.
[[nodiscard]]
std::expected<std::vector<StationData>, std::string>
fetch_weather(std::span<const std::string> codes);
