# database

## Purpose

RAII wrapper around SQLite3. On open, automatically creates the schema (tables + indexes) if they don't exist. Provides two insert methods — one per observation type. This is the sole write path to the SQLite file; the dashboard only reads from the same database.

## Key classes / functions

`Database` — RAII SQLite connection class — `src/database.hpp`

| Signature | Description |
|---|---|
| `Database::open(path)` | Static factory. Creates parent directories if needed, opens DB, initialises schema. Returns `std::expected<Database, std::string>` |
| `insert_weather(WeatherObs)` | Inserts one weather observation. `INSERT OR IGNORE` |
| `insert_air_quality(AqObs)` | Inserts one air quality observation. `INSERT OR IGNORE` |

**Data structs:**

`WeatherObs` — `{station_code, timestamp, obs_name, obs_value?, obs_unit}`  
`AqObs` — `{station_name, timestamp, pollutant, value?, unit}`

`obs_value` / `value` — `std::optional<double>`: `nullopt` → NULL in SQLite (station did not report a value).

**Table schema** (created in `Database::initialize()`):

```sql
weather_observations     (id, station_code, timestamp, obs_name, obs_value, obs_unit)
  UNIQUE(station_code, timestamp, obs_name)
  INDEX(station_code, timestamp)

air_quality_observations (id, station_name, timestamp, pollutant, value, unit)
  UNIQUE(station_name, timestamp, pollutant)
  INDEX(station_name, timestamp)
```

## Dependencies

Depends on: SQLite3 (system library)  
Depended on by: `main`

## How to add a feature

**Add a new table:**
1. In `Database::initialize()` at `src/database.cpp:56`, append `CREATE TABLE IF NOT EXISTS` and its index to the same SQL literal
2. In `src/database.hpp` declare a new data struct and an `insert_*` method
3. In `src/database.cpp` implement the method using `exec_insert(db_, kSql, ...)` — see lines 133–147 for the pattern

**Add a read method (SELECT):**
1. Declare in `database.hpp`
2. Implement directly with `sqlite3_prepare_v2` / `sqlite3_step` — the `exec_insert` template is INSERT-only

## How to diagnose a bug

- `Database::open()` returned an error → check directory permissions and available disk space
- `insert_*()` returned an error → the message contains `sqlite3_errmsg()` — read it literally
- Duplicates are silently ignored (`INSERT OR IGNORE`) — this is expected, not a bug

## Pitfalls

- `Database` is move-only; copy is deleted. Cannot store in containers by value without `std::move`
- `exec_insert` is a template function in an anonymous namespace (`database.cpp:89`) — inaccessible outside the file; for internal use only
- `Database::initialize()` runs all `CREATE TABLE` and `CREATE INDEX` statements in a single `sqlite3_exec` call — they execute sequentially; any new DDL added must be idempotent (`IF NOT EXISTS`)
- WAL mode is not enabled — brief lock contention is possible when the dashboard reads while the collector writes

## What not to change without discussion

- Table names `weather_observations` and `air_quality_observations` — external contract with the dashboard
- Column names and order in both tables
- `UNIQUE` constraints — they guarantee idempotency of repeated runs
