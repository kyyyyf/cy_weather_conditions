#include "database.hpp"
#include <format>
#include <unordered_map>

Database::~Database() { if (db_) sqlite3_close(db_); }

Database::Database(Database&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }

Database& Database::operator=(Database&& o) noexcept {
    if (this != &o) { if (db_) sqlite3_close(db_); db_ = o.db_; o.db_ = nullptr; }
    return *this;
}

std::expected<Database, std::string> Database::open(const std::filesystem::path& path) {
    sqlite3* db = nullptr;
    const int flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX;
    if (sqlite3_open_v2(path.c_str(), &db, flags, nullptr) != SQLITE_OK) {
        std::string msg = db ? sqlite3_errmsg(db) : "unknown error";
        sqlite3_close(db);
        return std::unexpected(std::format("Cannot open {}: {}", path.string(), msg));
    }
    return Database{db};
}

// ── Weather ───────────────────────────────────────────────────────────────────

std::expected<std::vector<TimeSeries>, std::string>
Database::fetch_weather(const std::string& station_code, int days) const {
    static constexpr char kSql[] =
        "SELECT obs_name, obs_value, obs_unit, timestamp"
        " FROM weather_observations"
        " WHERE station_code = ?"
        "   AND timestamp >= datetime('now', ?)"
        " ORDER BY obs_name, timestamp";

    const auto interval = std::format("-{} day", days);

    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, kSql, -1, &raw, nullptr) != SQLITE_OK)
        return std::unexpected(std::string{sqlite3_errmsg(db_)});

    struct StmtGuard { sqlite3_stmt* s; ~StmtGuard(){ sqlite3_finalize(s); } } guard{raw};

    sqlite3_bind_text(raw, 1, station_code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(raw, 2, interval.c_str(),     -1, SQLITE_TRANSIENT);

    std::unordered_map<std::string, std::pair<std::string, std::vector<DataPoint>>> map;

    while (sqlite3_step(raw) == SQLITE_ROW) {
        const auto name = std::string{reinterpret_cast<const char*>(sqlite3_column_text(raw, 0))};
        const auto unit = std::string{reinterpret_cast<const char*>(sqlite3_column_text(raw, 2))};
        const auto ts   = std::string{reinterpret_cast<const char*>(sqlite3_column_text(raw, 3))};
        std::optional<double> val;
        if (sqlite3_column_type(raw, 1) != SQLITE_NULL)
            val = sqlite3_column_double(raw, 1);

        auto& [stored_unit, pts] = map[name];
        if (stored_unit.empty()) stored_unit = unit;
        pts.push_back({ts, val});
    }

    std::vector<TimeSeries> result;
    result.reserve(map.size());
    for (auto& [name, pair] : map)
        result.push_back({name, pair.first, std::move(pair.second)});
    std::sort(result.begin(), result.end(),
        [](const TimeSeries& a, const TimeSeries& b){ return a.name < b.name; });

    return result;
}

// ── Air quality ───────────────────────────────────────────────────────────────

std::expected<std::vector<AqReading>, std::string>
Database::fetch_air_quality(const std::string& station_name) const {
    // Get pollutants at the most recent timestamp for this station.
    static constexpr char kSql[] =
        "SELECT pollutant, value, unit"
        " FROM air_quality_observations"
        " WHERE station_name = ?"
        "   AND timestamp = ("
        "       SELECT MAX(timestamp)"
        "         FROM air_quality_observations"
        "        WHERE station_name = ?)"
        " ORDER BY pollutant";

    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db_, kSql, -1, &raw, nullptr) != SQLITE_OK)
        return std::unexpected(std::string{sqlite3_errmsg(db_)});

    struct StmtGuard { sqlite3_stmt* s; ~StmtGuard(){ sqlite3_finalize(s); } } guard{raw};

    sqlite3_bind_text(raw, 1, station_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(raw, 2, station_name.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<AqReading> result;
    while (sqlite3_step(raw) == SQLITE_ROW) {
        const auto pollutant = std::string{reinterpret_cast<const char*>(sqlite3_column_text(raw, 0))};
        const auto unit      = std::string{reinterpret_cast<const char*>(sqlite3_column_text(raw, 2))};
        std::optional<double> val;
        if (sqlite3_column_type(raw, 1) != SQLITE_NULL)
            val = sqlite3_column_double(raw, 1);
        result.push_back({pollutant, unit, val});
    }

    return result;
}
