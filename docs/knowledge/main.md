# main

## Purpose

Entry point and orchestrator for a single data collection run. Loads config, opens the database, runs both collectors sequentially, and logs results. Contains no business logic — only wires modules together.

## Key functions

`main(argc, argv)` — entry point, `src/main.cpp:97`  
`collect_weather(db, codes, log)` — static function; calls `fetch_weather()` and writes results to DB — `src/main.cpp:14`  
`collect_air_quality(db, names, log)` — same for air quality — `src/main.cpp:56`

**Initialisation order:**
1. `Config::load(cfg_path)` — error → `return 1`
2. `Database::open(cfg->database_path)` — error → `return 1`
3. `Logger{cfg->log_file}` — constructor cannot fail
4. `collect_weather(...)` → `collect_air_quality(...)`

## Dependencies

Depends on: `config`, `database`, `logger`, `dom_collector`, `air_quality_collector`  
Depended on by: nothing (entry point)

## How to add a feature

**Add a third collector:**
1. Create `src/new_collector.hpp/.cpp` following the `dom_collector` pattern
2. Add `#include "new_collector.hpp"` to `main.cpp`
3. Add `static void collect_new(Database& db, ..., const Logger& log)` following lines 14–52
4. Call it from `main()` after `collect_air_quality`
5. Add `src/new_collector.cpp` to `add_executable()` in `CMakeLists.txt`

## How to diagnose a bug

- Init errors (`Config::load`, `Database::open`) → `stderr` + `return 1`; check process output
- Partial collector failures → log file; search for `"PARTIAL"`, `"failed"`, or `"ERROR"` lines
- `"station 'X' not found"` → station code or name in config does not match what the source returned

## Pitfalls

- Default config path is `"config.toml"` (relative). If the binary is run from outside `weather-collector-cpp/` — pass the path explicitly: `./build/weather-collector /abs/path/config.toml`
- Collectors run **sequentially** — a failure in one does not abort the other (no `return` after `collect_weather`)
- `collect_weather` and `collect_air_quality` emit a warning for each configured station absent from the response — this is diagnostic logic at the `main` level, not inside the collector

## What not to change without discussion

Initialisation order (Config → Database → Logger) — Logger is needed only after the DB is open; changing the order has side effects.
