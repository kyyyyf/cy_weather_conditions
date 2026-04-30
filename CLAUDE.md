# cy_weather_conditions / weather-collector-cpp

## Stack

| | |
|---|---|
| Language | C++26 |
| Compiler | GCC 14+ or Clang 17+ (Apple Clang — via fallback) |
| Build system | CMake ≥ 3.26 |
| System dependencies | libcurl, SQLite3 |
| FetchContent dependencies | toml++ v3.4.0, nlohmann/json v3.11.3, pugixml v1.14 |

## Commands

```bash
# ── Local (native) ────────────────────────────────────────────────────────────
# Release build
cmake -B weather-collector-cpp/build -S weather-collector-cpp -DCMAKE_BUILD_TYPE=Release
cmake --build weather-collector-cpp/build -j$(nproc)

# Debug build — enables ASan + UBSan
cmake -B weather-collector-cpp/build -S weather-collector-cpp -DCMAKE_BUILD_TYPE=Debug
cmake --build weather-collector-cpp/build

# Run (local)
cd weather-collector-cpp && ./build/weather-collector config.toml

# ── Docker (production) ───────────────────────────────────────────────────────
docker compose build          # build image (first time or after source changes)
docker compose run --rm collector   # run once manually
docker compose logs collector       # view logs from last run

# Tests — none exist (no tests/ directory)
```

Binary: `weather-collector-cpp/build/weather-collector`  
Docker config: `weather-collector-cpp/config.docker.toml`

## Project structure

| Directory / file | Contents |
|---|---|
| `weather-collector-cpp/src/` | All C++ source code |
| `weather-collector-cpp/CMakeLists.txt` | Build system, dependencies |
| `weather-collector-cpp/config.toml` | Default config (paths, station lists) |
| `db/` | Runtime SQLite database (only `.gitkeep` in git) |
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
