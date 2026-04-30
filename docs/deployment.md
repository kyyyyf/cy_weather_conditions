# Deployment

## Overview

Two separate Docker Compose stacks share a single named volume (`cy_weather_db`).
The collector writes to it every 30 minutes; the web app reads from it (read-only).

```
[host cron]
    └── docker compose run --rm collector
            └── writes /data/weather_conditions.db
                         │
                    cy_weather_db volume
                         │
            └── webapp container reads /data/weather_conditions.db (ro)
```

## Collector stack (this repo)

### First-time setup on the VPS

```bash
git clone <repo> /opt/cy_weather_conditions
cd /opt/cy_weather_conditions

# Build the image (takes a few minutes — downloads FetchContent deps)
docker compose build

# Smoke test — should log "collection started" and "collection complete"
docker compose run --rm collector
```

### Cron

Add to the VPS crontab (`crontab -e`):

```cron
*/30 * * * * cd /opt/cy_weather_conditions && docker compose run --rm collector >> /var/log/cy_weather.log 2>&1
```

`docker compose run --rm` starts a fresh container, runs the collector once, then removes the container. The named volume persists between runs.

### Updating

```bash
cd /opt/cy_weather_conditions
git pull
docker compose build
# Next cron tick picks up the new image automatically
```

---

## Web app stack (separate repo)

In the web app's `docker-compose.yml`, reference the collector's volume as external:

```yaml
volumes:
  cy_weather_db:
    external: true       # must already exist — created by the collector stack
    name: cy_weather_db

services:
  webapp:
    # ... your image / build ...
    volumes:
      - cy_weather_db:/data:ro
    environment:
      DB_PATH: /data/weather_conditions.db
```

The web app reads the SQLite file at `/data/weather_conditions.db`.  
Mount is read-only (`:ro`) — the web app must never write to this file.

> **Important:** start the collector stack first so the volume exists before the web app stack starts.

---

## Database schema reference

Tables written by the collector and read by the web app:

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
    pollutant    TEXT    NOT NULL,   -- ASCII: "PM10", "PM25", "NO2", "O3", "SO2", "CO", "C6H6"
    value        REAL,               -- NULL when station reports N/A
    unit         TEXT    NOT NULL,   -- "mg/m³" for CO, "μg/m³" otherwise
    UNIQUE(station_name, timestamp, pollutant)
)
```

Useful query patterns:

```sql
-- Latest reading per station (weather)
SELECT station_code, MAX(timestamp), obs_name, obs_value, obs_unit
FROM weather_observations
GROUP BY station_code, obs_name;

-- All air quality readings for the last hour
SELECT *
FROM air_quality_observations
WHERE timestamp >= datetime('now', '-1 hour');
```

---

## Logs

Container stdout/stderr is captured by Docker:

```bash
# Last run logs
docker compose logs collector

# Live tail during a manual run
docker compose run --rm collector
```

Persistent log file is also written to the shared volume at `/data/collector.log`.
