#pragma once
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <sqlite3.h>

struct WeatherObs {
    std::string station_code;
    std::string timestamp;    // "YYYY-MM-DD HH:MM"
    std::string obs_name;
    std::optional<double> obs_value;
    std::string obs_unit;
};

struct AqObs {
    std::string station_name;
    std::string timestamp;    // "YYYY-MM-DD HH:MM"
    std::string pollutant;
    std::optional<double> value;
    std::string unit;
};

class Database {
    sqlite3* db_ = nullptr;

    std::expected<void, std::string> initialize();
    std::expected<void, std::string> exec(const char* sql);

public:
    [[nodiscard]]
    static std::expected<Database, std::string> open(const std::filesystem::path& path);

    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;
    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;
    ~Database();

    [[nodiscard]] std::expected<void, std::string> insert_weather(const WeatherObs& obs);
    [[nodiscard]] std::expected<void, std::string> insert_air_quality(const AqObs& obs);

private:
    explicit Database(sqlite3* db) : db_(db) {}
};
