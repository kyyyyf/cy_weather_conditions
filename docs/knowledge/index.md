# Project map

## Quick start for an agent

Read in this order before any task:
1. `CLAUDE.md` — build commands, architectural invariants
2. `docs/knowledge/index.md` (this file) — overall map
3. The knowledge file for the module you need to change

To understand how everything connects: `docs/knowledge/main.md`  
For data / schema tasks: `docs/knowledge/database.md`

## Module map

| Module | Purpose | Key dependencies |
|--------|---------|-----------------|
| [main](main.md) | Entry point, orchestration | config, database, logger, both collectors |
| [config](config.md) | TOML config loading | toml++ |
| [logger](logger.md) | File + stdout/stderr logging | stdlib |
| [database](database.md) | SQLite RAII, schema, inserts | SQLite3 |
| [http](http.md) | Synchronous HTTP GET | libcurl |
| [dom_collector](dom_collector.md) | Weather from DOM.org.cy (XML) | http, pugixml |
| [air_quality_collector](air_quality_collector.md) | Air quality from REST JSON API | http, nlohmann/json |

## Typical pattern for adding a feature

**New data source (new collector):**
1. `src/new_collector.hpp` — declare data structs and `fetch_*()` function
2. `src/new_collector.cpp` — implementation via `http_get()` + parsing
3. `src/database.hpp/.cpp` — new obs struct + `insert_*()` method + table in `initialize()`
4. `src/main.cpp` — `collect_new()` function + call from `main()`
5. `CMakeLists.txt` — add `src/new_collector.cpp` to `add_executable()`
6. `config.toml` / `src/config.hpp/.cpp` — new config parameters

**New station of an existing type:**  
Only `config.toml` — add the code/name to the relevant array.

**New config parameter:**  
`src/config.hpp` (field) → `src/config.cpp` (parsing) → `config.toml` (example).

## Known project pitfalls

| Pitfall | Where |
|---|---|
| Config path and `database_path` are **relative to CWD**, not the binary | config, main |
| `INSERT OR IGNORE` — duplicate rows are silently ignored, this is normal | database |
| AQ API returns **2 hourly records** per station; both are written to the DB | air_quality_collector |
| Pollutant names contain unicode subscript digits → normalised to ASCII | air_quality_collector |
| `std::flat_set` unavailable on Apple Clang — `std::set` fallback used | dom_collector, air_quality_collector |
| `localtime_r` instead of `zoned_time` — workaround for Apple libc++ bug | logger, air_quality_collector |
| `Logger::write()` opens the file on every call (no cached fd) | logger |
| `trim_sv()` is duplicated in two files (no shared utils) | dom_collector, air_quality_collector |

## File layout

```
weather-collector-cpp/
  src/
    main.cpp               ← entry point
    config.hpp/.cpp        ← TOML parsing
    logger.hpp/.cpp        ← logging
    database.hpp/.cpp      ← SQLite + schema
    http.hpp/.cpp          ← libcurl GET
    dom_collector.hpp/.cpp ← XML parsing (weather)
    air_quality_collector.hpp/.cpp  ← JSON parsing (AQ)
  CMakeLists.txt           ← build system
  config.toml              ← config (production)
db/
  .gitkeep                 ← DB is created here at runtime
docs/
  knowledge/               ← this index + one file per module
  adr/                     ← architectural decision records
```
