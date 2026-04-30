# air_quality_collector

## Purpose

Queries the Cyprus Department of Labour REST API (airquality.dli.mlsi.gov.cy), retrieves air quality data for the past 2 hours, and returns readings for the configured stations. Pollutants: PM10, PM2.5, O3, NO2, SO2, CO, C6H6.

## Key classes / functions

`Measurement` — one measurement `{pollutant, value?, unit}` — `src/air_quality_collector.hpp`  
`StationReading` — one station's data `{station_name, timestamp, measurements[]}` — `src/air_quality_collector.hpp`  
`fetch_air_quality(names)` — downloads and parses JSON, returns `std::expected<vector<StationReading>, std::string>` — `src/air_quality_collector.hpp`

- `names` — `std::span<const std::string>`, station names from config (e.g. `"Nicosia - Traffic Station"`)
- Name matching is **case-insensitive** with whitespace normalisation via `normalise_name()`
- A single API call can return **multiple hourly records** per station (2-hour window)
- Units: CO → `"mg/m³"`, everything else → `"μg/m³"`

**Endpoint:** `GET /all_stations_data_range_PM/{from}/{to}` where `{from}` and `{to}` are `YYYY-MM-DD%20HH:00` (2 hours ago and now)

## Dependencies

Depends on: `http`, nlohmann/json v3.11.3  
Depended on by: `main`

## How to add a feature

**Add a new station:** edit `config.toml` — the name must match the `name_en` field in the JSON API (comparison is case-insensitive).

**Add a new pollutant:** no code changes needed — all `"pollutant_*"` keys in the JSON are iterated automatically in `parse_aq_json()` at `src/air_quality_collector.cpp:139`.

**Change the time window** (currently 2 hours):  
In `build_time_window()` at `src/air_quality_collector.cpp:93`, change `hours{2}` to the desired value.

## How to diagnose a bug

- `fetch_air_quality()` returned `unexpected` → network error, HTTP != 200, or JSON parse failure
- API returned `{"status": 0, "msg": "..."}` → server-side error; the msg text is forwarded as the unexpected value
- Station is in config but WARN `"station 'X' not found in response"` → name did not match; verify the `name_en` field in the raw JSON response with curl
- Null values are normal — a station may not report data for a specific hour

## Pitfalls

- Pollutant names in the API contain **unicode subscript digits** (U+2080–U+2089): `"SO₂"`, `"PM₁₀"`. `normalise_pollutant()` converts them to ASCII (`"SO2"`, `"PM10"`). The database stores normalised names
- The API's 2-hour window means **2 records** with different timestamps can arrive per station. Both are written to the DB
- Uses `localtime_r` (POSIX) to build the request time window — same reason as in logger (Apple libc++ workaround)
- `_` as a structured binding name (`for (const auto& [_, station])`) is a C++26 placeholder; may produce a warning on some C++23 compilers
- `trim_sv()` is duplicated from `dom_collector.cpp` — no shared utils file

## What not to change without discussion

`StationReading` and `Measurement` structs — used directly in `main` to call `db.insert_air_quality()`.  
Normalised pollutant names (`"PM10"`, `"NO2"`, etc.) — if the dashboard filters by these names, changing normalisation will break queries.
