# weather-collector

C++26 CLI that collects weather and air quality data for Cyprus and stores it in a shared SQLite database.

Runs every 30 minutes via cron. Part of the [cy_weather_conditions](https://github.com/kyyyf/cy_weather_conditions) project.

## Data sources

| Source | Protocol | What |
|--------|----------|------|
| [dom.org.cy](https://www.dom.org.cy) | XML feed | Temperature, humidity, wind (6 stations) |
| [airquality.dli.mlsi.gov.cy](https://www.airquality.dli.mlsi.gov.cy) | REST JSON | PM10, PM2.5, O3, NO2, SO2, CO, C6H6 (5 stations) |

## Requirements

| Tool | Version |
|------|---------|
| CMake | ≥ 3.26 |
| C++ compiler | C++23 support (GCC 14+ or Clang 17+) |
| libcurl | any recent |
| SQLite3 | any recent |

Dependencies fetched automatically via CMake FetchContent:
[toml++](https://github.com/marzer/tomlplusplus) · [nlohmann/json](https://github.com/nlohmann/json) · [pugixml](https://github.com/zeux/pugixml)

### macOS
```bash
brew install cmake curl sqlite3
```

### Debian / Ubuntu
```bash
apt install cmake libcurl4-openssl-dev libsqlite3-dev
```

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Binary: `build/weather-collector`

## Configuration

Copy and edit `config.toml`:

```toml
database_path = "/path/to/weather_conditions.db"

[weather_stations]
codes = ["ATHALASSA", "LCLK", "PAPHOS", "LIMASSOL", "CAVO_GRECO"]

[air_quality_stations]
names = [
    "Nicosia - Traffic Station",
    "Larnaca - Traffic Station",
    "Paphos - Traffic Station",
    "Limassol - Traffic Station",
    "Paralimni - Traffic Station",
]
```

`log_file` is optional (default: `collector.log` in CWD).

## Usage

```bash
./build/weather-collector config.toml
```

Must be run from the directory containing `config.toml`, or pass the path explicitly.

## Cron

```cron
*/30 * * * * cd /path/to/weather-collector && ./build/weather-collector config.toml
```

## Database schema

Two tables written by this collector (read by the dashboard):

```
weather_observations     (station_code, timestamp, obs_name, obs_value, obs_unit)
air_quality_observations (station_name, timestamp, pollutant, value, unit)
```

Both use `INSERT OR IGNORE` — safe to run concurrently with the dashboard reader.
