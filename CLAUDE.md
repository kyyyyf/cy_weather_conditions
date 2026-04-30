# cy_weather_conditions

## Stack

| | |
|---|---|
| Language | C++23 |
| Compiler | GCC 13+ or Clang 17+ (Apple Clang — via fallback) |
| Build system | CMake ≥ 3.26 + root-level `Makefile` |
| Collector deps | libcurl, SQLite3, toml++ v3.4.0, nlohmann/json v3.11.3, pugixml v1.14 |
| Dashboard deps | SQLite3, Crow v1.2.0, Inja v3.4.0, Asio, toml++ v3.4.0, nlohmann/json v3.11.3 |

## Commands

```bash
# ── Build both (from repo root) ───────────────────────────────────────────────
make                          # Release (default)
make BUILD_TYPE=Debug         # Debug — enables ASan + UBSan
make CC=gcc-14 CXX=g++-14     # explicit compiler version
make collector                # collector only
make dashboard                # dashboard only
make clean                    # remove both build dirs

# ── Run (from repo root) ──────────────────────────────────────────────────────
cd weather-collector-cpp && ./build/weather-collector config.toml
cd weather-dashboard-cpp && ./build/weather-dashboard config.toml

# ── Tests — none exist (no tests/ directory) ──────────────────────────────────
```

Binaries:
- `weather-collector-cpp/build/weather-collector`
- `weather-dashboard-cpp/build/weather-dashboard`

## Project structure

| Directory / file | Contents |
|---|---|
| `weather-collector-cpp/src/` | Collector C++ source |
| `weather-collector-cpp/CMakeLists.txt` | Collector build, dependencies |
| `weather-collector-cpp/config.toml` | Collector config (DB path, station lists) |
| `weather-dashboard-cpp/src/` | Dashboard C++ source |
| `weather-dashboard-cpp/CMakeLists.txt` | Dashboard build, dependencies |
| `weather-dashboard-cpp/config.toml` | Dashboard config (DB path, port, locations) |
| `weather-dashboard-cpp/templates/` | Inja HTML templates |
| `weather-dashboard-cpp/static/` | CSS + JS assets |
| `db/` | Runtime SQLite database (only `.gitkeep` in git) |
| `deploy/` | systemd unit file for the dashboard |
| `docs/knowledge/` | Per-module knowledge files |
| `docs/adr/` | Architectural decision records |

## Coding conventions

- All error-returning functions use `std::expected<T, std::string>` — no exceptions thrown
- All public functions are marked `[[nodiscard]]`
- Timestamps stored as `TEXT` in `"YYYY-MM-DD HH:MM"` format
- Observation values are `std::optional<double>` (nullopt = station reported N/A)
- Naming: `snake_case` for variables and functions, `PascalCase` for types/classes
- Header files use `#pragma once`
- Private class members use trailing `_` suffix (e.g. `db_`, `path_`)

## Architectural invariants

- **`INSERT OR IGNORE`** in both insert methods — do not change to `INSERT` or `REPLACE`; idempotency is intentional for safe re-runs
- Public database interface is only `insert_weather(WeatherObs)` and `insert_air_quality(AqObs)`. The dashboard reads the same DB — the table schema is a public contract
- Timestamp format `"YYYY-MM-DD HH:MM"` is shared between the collector and dashboard — do not change
- Air quality station names in config must match `name_en` field in the API response (comparison is case-insensitive inside `fetch_air_quality`)

## Do not touch without explicit instruction

- `weather-collector-cpp/build/` — generated directory, not in git
- Table schema in `Database::initialize()` (`database.cpp:56`) — read by external dashboard
- `config.toml` — production config with real paths and station lists
- `db/` — SQLite file is created at runtime; directory is intentionally empty in the repository
