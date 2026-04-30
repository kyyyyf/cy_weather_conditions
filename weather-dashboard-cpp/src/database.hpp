#pragma once
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <sqlite3.h>

struct DataPoint {
    std::string timestamp;       // "YYYY-MM-DD HH:MM"
    std::optional<double> value;
};

struct TimeSeries {
    std::string name;
    std::string unit;
    std::vector<DataPoint> data;
};

struct AqReading {
    std::string pollutant;       // ASCII-normalised: "PM10", "NO2", …
    std::string unit;
    std::optional<double> value;
};

class Database {
    sqlite3* db_ = nullptr;
    explicit Database(sqlite3* db) : db_(db) {}

public:
    [[nodiscard]]
    static std::expected<Database, std::string> open(const std::filesystem::path& path);

    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;
    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;
    ~Database();

    // Returns all weather series for the station over the last `days` days.
    [[nodiscard]]
    std::expected<std::vector<TimeSeries>, std::string>
    fetch_weather(const std::string& station_code, int days = 1) const;

    // Returns the latest value per pollutant for the station.
    [[nodiscard]]
    std::expected<std::vector<AqReading>, std::string>
    fetch_air_quality(const std::string& station_name) const;
};
