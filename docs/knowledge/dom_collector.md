# dom_collector

## Purpose

Downloads the XML feed from the Cyprus Department of Meteorology (DOM, dom.org.cy) and returns weather observations for the requested stations. Data includes temperature, humidity, wind speed, and other parameters reported by DOM. A single XML document covers all stations on the island — this module filters for the configured ones.

## Key classes / functions

`Observation` — one measurement `{name, value?, unit}` — `src/dom_collector.hpp`  
`StationData` — one station's data `{station_code, timestamp, observations[]}` — `src/dom_collector.hpp`  
`fetch_weather(codes)` — downloads and parses the XML, returns `std::expected<vector<StationData>, std::string>` — `src/dom_collector.hpp`

- `codes` — `std::span<const std::string>`, station codes from config (e.g. `"ATHALASSA"`, `"LCLK"`)
- Returns only stations whose codes are in `codes`; missing stations trigger a WARN log from `main`

**Feed URL:** `https://www.dom.org.cy/AWS/OpenData/CyDoM.xml`

## Dependencies

Depends on: `http`, pugixml v1.14  
Depended on by: `main`

## How to add a feature

**Add a new station:** edit `config.toml` only — codes come from config, no code changes needed.

**Handle a new XML field:**  
In `parse_xml()` at `src/dom_collector.cpp:68`, `<observation>` nodes are already iterated automatically — all fields in a station block are parsed without code changes. To handle a specific `observation_name` differently, add a condition inside the `for (pugi::xml_node obs : node.children("observation"))` loop.

**Change the data source** (new URL or format):  
1. Update `kDomUrl` at `src/dom_collector.cpp:17`
2. Rewrite `parse_xml()` for the new XML structure

## How to diagnose a bug

- `fetch_weather()` returned `unexpected` → network error or `"XML parse error: ..."` (malformed XML from server)
- Station is in config but WARN `"station 'X' not found in XML"` → station code doesn't match the XML (typo, wrong case)
- Null value (N/A) — expected: `parse_double()` returns `nullopt` for empty strings

## Pitfalls

- Timestamps in the XML include a `" (Local Time)"` suffix — `normalise_dom_timestamp()` strips it. If DOM changes the format, the suffix may silently appear in stored timestamps
- `std::flat_set` / `std::set` fallback (lines 8–13) — on Apple Clang `<flat_set>` is unavailable, so `std::set` is used. Behaviour is identical, performance is slightly worse
- `trim_sv()` is duplicated between `dom_collector.cpp` and `air_quality_collector.cpp` — intentional, there is no shared utils file

## What not to change without discussion

`StationData` and `Observation` structs — used directly in `main` to call `db.insert_weather()`.
