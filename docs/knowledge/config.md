# config

## Purpose

Loads and validates the TOML configuration at startup. Provides the `Config` struct with paths and station lists to all other modules. The program does not start if loading fails.

## Key classes / functions

`Config` — plain struct holding all config fields — `src/config.hpp`  
`Config::load(path)` — static factory method; parses the TOML file, returns `std::expected<Config, std::string>` — `src/config.cpp`

**`Config` fields:**

| Field | Type | Required | Default |
|-------|------|----------|---------|
| `database_path` | `std::filesystem::path` | yes | — |
| `log_file` | `std::filesystem::path` | no | `"collector.log"` |
| `weather_codes` | `std::vector<std::string>` | yes | — |
| `aq_names` | `std::vector<std::string>` | yes | — |

## Dependencies

Depends on: toml++ v3.4.0 (FetchContent)  
Depended on by: `main`

## How to add a feature

**Add a new config parameter:**
1. Add the field to `Config` in `src/config.hpp`
2. In `src/config.cpp` inside `Config::load()`, parse the value from `tbl` — see lines 17–56 for examples
3. Required fields: `return std::unexpected(...)` if the value is absent
4. Optional fields: wrap in `if (auto val = tbl["key"].value<T>())`
5. Update `config.toml` with an example of the new key

## How to diagnose a bug

- Parse errors are printed to `stderr` via `std::println` in `main.cpp:101`
- If `load()` returns `unexpected` — the error text names the missing key
- Check that all three required sections are present: `database_path`, `[weather_stations].codes`, `[air_quality_stations].names`

## Pitfalls

- `Config::load()` takes `string_view path` — the path is resolved relative to the **current working directory**, not the binary. README warns: run from the directory that contains `config.toml`
- `database_path` in the default `config.toml` is a relative path `"../db/weather_conditions.db"` — also depends on CWD

## What not to change without discussion

TOML key names (`database_path`, `log_file`, `weather_stations.codes`, `air_quality_stations.names`) — production deployment configs depend on these names.
