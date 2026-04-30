# Deployment

## Overview

Both binaries run natively on the VPS. The collector is invoked by cron every 30 minutes; the dashboard runs as a systemd service on port 3000. They share a single SQLite file.

```
[host cron every 30 min]
    └── weather-collector config.toml
            └── writes /opt/cy_weather_conditions/db/weather_conditions.db
                                  │
                        shared SQLite file (same filesystem)
                                  │
    [systemd: weather-dashboard]
            └── reads db (read-only)  →  serves http://localhost:3000
```

---

## System dependencies (Ubuntu 24.04)

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake ninja-build \
    gcc g++ \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    git ca-certificates
```

> On Ubuntu 22.04 add the `ubuntu-toolchain-r/test` PPA to get gcc-13+.

---

## First-time setup on the VPS

```bash
git clone <repo> /opt/cy_weather_conditions
cd /opt/cy_weather_conditions

# Build both binaries (Release)
make -j$(nproc)

# Create the DB directory (gitignored, empty)
mkdir -p db

# Smoke test — should print "collection complete"
cd weather-collector-cpp && ./build/weather-collector config.toml
```

Binary locations after build:
- `weather-collector-cpp/build/weather-collector`
- `weather-dashboard-cpp/build/weather-dashboard`

---

## Collector — cron

Add to the VPS crontab (`crontab -e`):

```cron
*/30 * * * * cd /opt/cy_weather_conditions/weather-collector-cpp && ./build/weather-collector config.toml >> /var/log/cy_weather_collector.log 2>&1
```

---

## Dashboard — systemd service

```bash
# Copy the unit file
sudo cp /opt/cy_weather_conditions/deploy/weather-dashboard.service \
        /etc/systemd/system/

# Reload and enable
sudo systemctl daemon-reload
sudo systemctl enable --now weather-dashboard

# Check status
sudo systemctl status weather-dashboard
sudo journalctl -u weather-dashboard -f
```

The service runs as `www-data`. If you deploy under a different user, edit the `User=` line in the unit file.

---

## Updating

```bash
cd /opt/cy_weather_conditions
git pull
make -j$(nproc)
sudo systemctl restart weather-dashboard
# Next cron tick picks up the new collector binary automatically
```

---

## Config paths

Both projects ship with a `config.toml` whose `database_path` is set to `../db/weather_conditions.db` (relative to the project subdirectory). This resolves to `/opt/cy_weather_conditions/db/weather_conditions.db` when the binary is run from its project directory, which is what the cron line and the systemd `WorkingDirectory` both ensure.

To use an absolute path instead, edit the `database_path` value in each `config.toml`.

---

## Database schema reference

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

## Logs

- Collector: `/var/log/cy_weather_collector.log` (written by cron redirection)
- Dashboard: `journalctl -u weather-dashboard`
