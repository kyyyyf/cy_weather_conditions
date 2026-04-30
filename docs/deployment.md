# Deployment

## Overview

The collector runs once per cron tick and writes to a SQLite file. The dashboard is a persistent HTTP server that reads the same file.

```
cron (every 30 min)
  └── weather-collector config.toml
        └── writes  /opt/cy_weather_conditions/db/weather_conditions.db

systemd (always on)
  └── weather-dashboard config.toml
        └── reads   /opt/cy_weather_conditions/db/weather_conditions.db (ro)
        └── serves  http://0.0.0.0:3000
```

---

## 1. Dependencies

### Required packages

| Package | What it provides | Min version |
|---------|-----------------|-------------|
| `cmake` | Build system | 3.26 |
| `ninja-build` | Build backend | any |
| `gcc-14` | C++ compiler (C++23) | 14 |
| `g++-14` | C++ compiler (C++23) | 14 |
| `libsqlite3-dev` | SQLite headers + library | any |
| `libcurl4-openssl-dev` | curl headers + library | any |
| `git` | FetchContent downloads | any |
| `ca-certificates` | HTTPS for FetchContent | any |

### Check what is installed

```bash
dpkg -l cmake ninja-build gcc-14 g++-14 libsqlite3-dev libcurl4-openssl-dev git ca-certificates \
  | awk '/^[uirc]/{print $1, $2}'
# "ii" = installed  "un" or missing line = not installed
```

Quick version check:

```bash
cmake --version          # need >= 3.26
gcc-14 --version
sqlite3 --version
curl --version
```

### Install missing packages

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake ninja-build \
    gcc-14 g++-14 \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    git ca-certificates
```

> On Ubuntu 22.04, GCC 14 is not in the default repos. Add the toolchain PPA first:
> ```bash
> sudo add-apt-repository ppa:ubuntu-toolchain-r/test
> sudo apt-get update
> ```

> **If cmake reports a library as not found after you install it**, delete the build
> directory and re-run cmake — it caches "not found" results and will keep failing
> until the cache is cleared:
> ```bash
> rm -rf weather-collector-cpp/build weather-dashboard-cpp/build
> ```

---

## 2. Build

Clone the repo and build both binaries:

```bash
git clone <repo-url> /opt/cy_weather_conditions
cd /opt/cy_weather_conditions

cmake -B weather-collector-cpp/build -S weather-collector-cpp \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc-14 \
    -DCMAKE_CXX_COMPILER=g++-14
cmake --build weather-collector-cpp/build -j$(nproc)

cmake -B weather-dashboard-cpp/build -S weather-dashboard-cpp \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc-14 \
    -DCMAKE_CXX_COMPILER=g++-14
cmake --build weather-dashboard-cpp/build -j$(nproc)
```

First build downloads FetchContent dependencies (~300 MB) and takes a few minutes. Subsequent builds are incremental.

### After build — binary locations

| Binary | Path |
|--------|------|
| Collector | `weather-collector-cpp/build/weather-collector` |
| Dashboard | `weather-dashboard-cpp/build/weather-dashboard` |

The dashboard binary must be run from the `weather-dashboard-cpp/` subdirectory because it resolves `templates/` and `static/` relative to the working directory. Do not move the binary out of the project tree.

The collector binary is standalone — it only needs a path to `config.toml`.

### Create the database directory

```bash
mkdir -p /opt/cy_weather_conditions/db
```

---

## 3. Configuration

Both projects ship with a `config.toml` where `database_path = "../db/weather_conditions.db"`.  
This resolves to `/opt/cy_weather_conditions/db/weather_conditions.db` when each binary is run from its own subdirectory, which is what the cron entry and systemd unit both ensure.

To use a different path, edit `database_path` in each `config.toml` before building (or after — it is read at runtime).

---

## 4. Cron — collector

Run the collector every 30 minutes. Open crontab:

```bash
crontab -e
```

Add:

```cron
*/30 * * * * cd /opt/cy_weather_conditions/weather-collector-cpp && ./build/weather-collector config.toml >> /var/log/cy_weather_collector.log 2>&1
```

The `cd` ensures the working directory is the project subdirectory so relative paths in `config.toml` resolve correctly.

Test that the binary runs before the first cron tick:

```bash
cd /opt/cy_weather_conditions/weather-collector-cpp
./build/weather-collector config.toml
```

Expected output ends with a line like `collection complete`.

---

## 5. Dashboard — systemd service

Install the bundled unit file:

```bash
sudo cp /opt/cy_weather_conditions/deploy/weather-dashboard.service \
        /etc/systemd/system/

sudo systemctl daemon-reload
sudo systemctl enable --now weather-dashboard
```

Check it started:

```bash
sudo systemctl status weather-dashboard
sudo journalctl -u weather-dashboard -f
```

The service's `WorkingDirectory` is set to `weather-dashboard-cpp/` so templates and static assets are found automatically.

### Running manually (foreground, for testing)

```bash
cd /opt/cy_weather_conditions/weather-dashboard-cpp
./build/weather-dashboard config.toml
```

---

## 6. Updating

```bash
cd /opt/cy_weather_conditions
git pull
cmake --build weather-collector-cpp/build -j$(nproc)
cmake --build weather-dashboard-cpp/build -j$(nproc)
sudo systemctl restart weather-dashboard
# collector picks up the new binary on the next cron tick
```

If CMakeLists.txt changed, re-run the full `cmake -B ... && cmake --build ...` commands from step 2.

---

## 7. Database schema

Tables written by the collector and read by the dashboard:

```sql
weather_observations (
    id           INTEGER PRIMARY KEY,
    station_code TEXT    NOT NULL,
    timestamp    TEXT    NOT NULL,   -- "YYYY-MM-DD HH:MM"
    obs_name     TEXT    NOT NULL,
    obs_value    REAL,               -- NULL when station reports N/A
    obs_unit     TEXT    NOT NULL,
    UNIQUE(station_code, timestamp, obs_name)
)

air_quality_observations (
    id           INTEGER PRIMARY KEY,
    station_name TEXT    NOT NULL,
    timestamp    TEXT    NOT NULL,   -- "YYYY-MM-DD HH:MM"
    pollutant    TEXT    NOT NULL,   -- "PM10", "PM25", "NO2", "O3", "SO2", "CO", "C6H6"
    value        REAL,               -- NULL when station reports N/A
    unit         TEXT    NOT NULL,   -- "mg/m³" for CO, "μg/m³" otherwise
    UNIQUE(station_name, timestamp, pollutant)
)
```

---

## 8. Logs

| Source | Location |
|--------|----------|
| Collector | `/var/log/cy_weather_collector.log` (cron redirect) |
| Dashboard | `journalctl -u weather-dashboard` |
