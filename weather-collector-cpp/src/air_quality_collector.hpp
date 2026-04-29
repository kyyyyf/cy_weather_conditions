#pragma once
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct Measurement {
    std::string           pollutant;  // ASCII-normalised: "PM10", "NO2", …
    std::optional<double> value;
    std::string           unit;       // "mg/m³" for CO, "μg/m³" otherwise
};

struct StationReading {
    std::string              station_name;
    std::string              timestamp;   // "YYYY-MM-DD HH:MM"
    std::vector<Measurement> measurements;
};

/// Fetches the Cyprus AQ REST API (2-hour window) and returns readings
/// for the requested station names (matched case-insensitively).
[[nodiscard]]
std::expected<std::vector<StationReading>, std::string>
fetch_air_quality(std::span<const std::string> names);
