#include "database.hpp"
#include <filesystem>
#include <format>

// ── RAII stmt handle ──────────────────────────────────────────────────────────

struct Stmt {
    sqlite3_stmt* ptr = nullptr;
    explicit Stmt(sqlite3_stmt* s) : ptr(s) {}
    ~Stmt() { if (ptr) sqlite3_finalize(ptr); }
    Stmt(const Stmt&)            = delete;
    Stmt& operator=(const Stmt&) = delete;
};

// ── Database ──────────────────────────────────────────────────────────────────

Database::~Database() { if (db_) sqlite3_close(db_); }

Database::Database(Database&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }

Database& Database::operator=(Database&& o) noexcept {
    if (this != &o) { if (db_) sqlite3_close(db_); db_ = o.db_; o.db_ = nullptr; }
    return *this;
}

std::expected<Database, std::string> Database::open(const std::filesystem::path& path) {
    if (auto parent = path.parent_path(); !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) return std::unexpected(
            std::format("Cannot create directories for {}: {}", path.string(), ec.message()));
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::string msg = sqlite3_errmsg(db);
        sqlite3_close(db);
        return std::unexpected(std::format("Cannot open {}: {}", path.string(), msg));
    }

    Database d{db};
    if (auto r = d.initialize(); !r) return std::unexpected(r.error());
    return d;
}

std::expected<void, std::string> Database::exec(const char* sql) {
    char* errmsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        return std::unexpected(msg);
    }
    return {};
}

std::expected<void, std::string> Database::initialize() {
    return exec(R"sql(
        CREATE TABLE IF NOT EXISTS weather_observations (
            id           INTEGER PRIMARY KEY,
            station_code TEXT    NOT NULL,
            timestamp    TEXT    NOT NULL,
            obs_name     TEXT    NOT NULL,
            obs_value    REAL,
            obs_unit     TEXT    NOT NULL DEFAULT '',
            UNIQUE(station_code, timestamp, obs_name)
        );
        CREATE INDEX IF NOT EXISTS idx_weather_station_time
            ON weather_observations(station_code, timestamp);
        CREATE TABLE IF NOT EXISTS air_quality_observations (
            id           INTEGER PRIMARY KEY,
            station_name TEXT    NOT NULL,
            timestamp    TEXT    NOT NULL,
            pollutant    TEXT    NOT NULL,
            value        REAL,
            unit         TEXT    NOT NULL DEFAULT '',
            UNIQUE(station_name, timestamp, pollutant)
        );
        CREATE INDEX IF NOT EXISTS idx_aq_station_time
            ON air_quality_observations(station_name, timestamp);
    )sql");
}

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace {

// Prepare + bind + step in one go. Returns unexpected on any SQLite error.
template<typename... Args>
std::expected<void, std::string> exec_insert(
    sqlite3* db, const char* sql, Args&&... args)
{
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK)
        return std::unexpected(std::string{sqlite3_errmsg(db)});

    Stmt stmt{raw};
    int idx = 1;

    // Fold over arguments to bind each one
    auto bind_one = [&](auto&& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        int rc = SQLITE_OK;
        if constexpr (std::same_as<T, std::optional<double>>) {
            rc = v ? sqlite3_bind_double(stmt.ptr, idx, *v)
                   : sqlite3_bind_null(stmt.ptr, idx);
        } else if constexpr (std::floating_point<T>) {
            rc = sqlite3_bind_double(stmt.ptr, idx, static_cast<double>(v));
        } else {
            // std::string or const char*
            const char* s;
            if constexpr (std::same_as<T, std::string>)
                s = v.c_str();
            else
                s = v;
            rc = sqlite3_bind_text(stmt.ptr, idx, s, -1, SQLITE_TRANSIENT);
        }
        ++idx;
        return rc == SQLITE_OK;
    };

    bool ok = (bind_one(std::forward<Args>(args)) && ...);
    if (!ok) return std::unexpected(std::string{sqlite3_errmsg(db)});

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        return std::unexpected(std::string{sqlite3_errmsg(db)});
    return {};
}

} // namespace

// ── Public insert methods ─────────────────────────────────────────────────────

std::expected<void, std::string> Database::insert_weather(const WeatherObs& o) {
    static constexpr char kSql[] =
        "INSERT OR IGNORE INTO weather_observations"
        " (station_code,timestamp,obs_name,obs_value,obs_unit)"
        " VALUES(?,?,?,?,?)";
    return exec_insert(db_, kSql, o.station_code, o.timestamp, o.obs_name, o.obs_value, o.obs_unit);
}

std::expected<void, std::string> Database::insert_air_quality(const AqObs& o) {
    static constexpr char kSql[] =
        "INSERT OR IGNORE INTO air_quality_observations"
        " (station_name,timestamp,pollutant,value,unit)"
        " VALUES(?,?,?,?,?)";
    return exec_insert(db_, kSql, o.station_name, o.timestamp, o.pollutant, o.value, o.unit);
}
