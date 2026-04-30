# Cyprus Weather Dashboard — HTMX Specification

## What the existing WASM app does (reference)

Single-page app built with Rust/Leptos (WASM) + Axum backend.

**User flow:**
1. Page loads → dropdown populated from `/api/locations`
2. User picks a city → app fetches weather + air quality for that city (`period=1d`)
3. Page renders:
   - **Current conditions card** — current temperature, daily max/min with time, humidity, wind direction + speed, worst AQ level badge, PM10/PM2.5 inline
   - **Air quality table** — all 7 pollutants (PM10, PM2.5, O3, NO2, SO2, CO, C6H6) with value, unit, and colored level label
   - **3 ECharts line charts** — Temperature (24h), Humidity (24h), Wind Speed with direction arrows (24h)

**5 cities (hardcoded mapping in config.toml):**

| City | Weather station code | AQ station name |
|------|---------------------|-----------------|
| Nicosia | `ATHALASSA` | `Nicosia - Traffic Station` |
| Larnaca | `LCLK` | `Larnaca - Traffic Station` |
| Limassol | `LIMASSOL` | `Limassol - Traffic Station` |
| Paphos | `PAPHOS` | `Paphos - Traffic Station` |
| Agia Napa | `CAVO_GRECO` | `Paralimni - Traffic Station` |

**AQ level classification (same thresholds in config):**

| Pollutant | Good | Moderate | High | Very High |
|-----------|------|----------|------|-----------|
| PM10 | < 50 | < 100 | < 200 | ≥ 200 |
| PM2.5 | < 25 | < 50 | < 100 | ≥ 100 |
| O3 | < 100 | < 140 | < 180 | ≥ 180 |
| NO2 | < 100 | < 150 | < 200 | ≥ 200 |
| SO2 | < 150 | < 250 | < 350 | ≥ 350 |
| CO | < 7000 | < 15000 | < 20000 | ≥ 20000 |
| C6H6 | < 5 | < 10 | < 15 | ≥ 15 |

Colors: Good `#4caf50` · Moderate `#ffc107` · High `#ff5722` · Very High `#f44336`

**Pollutant display order:** PM10, PM2.5, O3, NO2, NOx, NO, SO2, CO, C6H6

---

## HTMX architecture

```
Browser                 Backend (any language with SQLite access)
───────                 ────────────────────────────────────────
GET /                ←→ full page HTML (shell + empty content area)
                         ↓  user picks city
hx-get /dashboard       server queries SQLite, renders HTML fragment
hx-target="#content" ←→ complete dashboard section HTML
hx-swap innerHTML

hx-trigger="every 5m"   auto-refresh the same fragment
```

The backend renders **complete HTML fragments** — no JSON, no client-side data processing.  
ECharts is initialized from `data-*` attributes injected by the server into the HTML.

---

## Routes

### `GET /`

Returns the full page shell. Content area is empty.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Cyprus Weather & Air Quality</title>
  <link rel="stylesheet" href="/static/style.css">
  <script src="https://cdn.jsdelivr.net/npm/echarts@5/dist/echarts.min.js"></script>
  <script src="/static/charts.js" defer></script>
</head>
<body>
  <div class="app">
    <header>
      <h1>Cyprus Weather &amp; Air Quality</h1>
      <span class="subtitle">Real-time conditions by location</span>
    </header>
    <main class="main">
      <div class="loc-bar">
        <label class="loc-label" for="loc-select">Location</label>
        <select id="loc-select" class="loc-select"
          hx-get="/dashboard"
          hx-target="#content"
          hx-swap="innerHTML"
          hx-trigger="change"
          hx-include="[name='location']"
          name="location">
          <option value="">— Select a city —</option>
          <option value="Nicosia">Nicosia</option>
          <option value="Larnaca">Larnaca</option>
          <option value="Limassol">Limassol</option>
          <option value="Paphos">Paphos</option>
          <option value="Agia Napa">Agia Napa</option>
        </select>
      </div>
      <div id="content" class="content-area">
        <div class="placeholder">
          <span class="icon">☀</span>
          <span>Select a location above</span>
        </div>
      </div>
    </main>
  </div>
</body>
</html>
```

---

### `GET /dashboard?location={city}`

Returns an HTML fragment that replaces `#content`. Backend queries SQLite for the last 24 hours.

**Query parameters:**
- `location` — city name, maps to weather_code + aq_station via config

**Server-side logic before rendering:**
1. Look up `(weather_code, aq_station)` from the location name in config
2. Query `weather_observations WHERE station_code = ? AND timestamp >= now - 1 day`
3. Query `air_quality_observations WHERE station_name = ? AND timestamp >= now - 2 hours` (latest reading only)
4. Compute from weather series:
   - `temp_cur` — last non-null value in "Air Temperature (1.2m)"
   - `temp_max` — max value + its time (HH:MM)
   - `temp_min` — min value + its time (HH:MM)
   - `humid_cur` — last non-null value in "Relative Humidity"
   - `wind_speed` — last non-null value in "Wind Speed (10m)"
   - `wind_dir` — last non-null value in "Wind Direction (10m)" → converted to compass (N/NNE/NE/…)
5. Compute AQ levels per pollutant using thresholds from config
6. Compute `worst_aq_level` = max level across all pollutants
7. Build chart data arrays (timestamp + value pairs) for Temperature, Humidity, Wind Speed + Direction

**Response shape:**

```html
<div class="dashboard"
  hx-get="/dashboard"
  hx-target="#content"
  hx-swap="innerHTML"
  hx-trigger="every 5m"
  hx-vals='{"location": "Nicosia"}'>

  <!-- Row 1: current conditions + AQ table -->
  <div class="top-row">

    <!-- Current conditions card -->
    <div class="cond-card">
      <div class="cond-top">
        <span class="cond-picto">🌡</span>
        <span class="cond-temp-val">{temp_cur}°C</span>   <!-- e.g. "24.3°C" or "—" -->
        <div class="curr-mm">
          <span class="mm-item hi">
            <span class="mm-arrow">▲</span>
            {temp_max_val}°C
            <span class="mm-time">{temp_max_time}</span>  <!-- "14:20" -->
          </span>
          <span class="mm-item lo">
            <span class="mm-arrow">▼</span>
            {temp_min_val}°C
            <span class="mm-time">{temp_min_time}</span>
          </span>
        </div>
      </div>
      <div class="cond-bottom">
        <div class="cond-sub">
          <span class="cond-picto">💧</span>
          <span class="cond-sub-val">{humid_cur}%</span>
        </div>
        <div class="cond-sub">
          <span class="cond-picto">💨</span>
          <span class="cond-sub-val">{compass}</span>      <!-- "SW" -->
          <span class="cond-sub-label">{wind_speed} m/s</span>
        </div>
        <div class="cond-sub">
          <!-- worst AQ level badge -->
          <span class="aq-badge-main" style="background:{worst_color}">{worst_label}</span>
          <div class="aq-pm-rows">
            <div class="aq-pm-row">
              <span class="aq-dot" style="background:{pm10_color}"></span>
              <span class="aq-pm-name">PM₁₀</span>
              <span class="aq-pm-val">{pm10_val} μg/m³</span>
            </div>
            <div class="aq-pm-row">
              <span class="aq-dot" style="background:{pm25_color}"></span>
              <span class="aq-pm-name">PM₂.₅</span>
              <span class="aq-pm-val">{pm25_val} μg/m³</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- AQ detail table -->
    <div class="data-card aq-detail">
      <div class="card-title">Air Quality — All Pollutants</div>
      <table class="aq-table">
        <thead>
          <tr><th>Pollutant</th><th>Value</th><th>Unit</th><th>Index</th></tr>
        </thead>
        <tbody>
          <!-- repeat for each pollutant in display order -->
          <tr>
            <td>{pollutant_name}</td>
            <td class="num">{value}</td>       <!-- "32.1" or "—" -->
            <td>{unit}</td>
            <td><span class="aq-lbl" style="color:{level_color}">{level_label}</span></td>
          </tr>
        </tbody>
      </table>
    </div>

  </div>

  <!-- Row 2: ECharts containers with data embedded as JSON in data attributes -->
  <div class="charts-row">

    <div class="data-card chart-card">
      <div class="card-title">🌡 Temperature</div>
      <div class="mini-chart"
        data-chart="line"
        data-color="#4fc3f7"
        data-unit="°C"
        data-points='{JSON array of [timestamp, value] pairs}'
      ></div>
    </div>

    <div class="data-card chart-card">
      <div class="card-title">💧 Humidity</div>
      <div class="mini-chart"
        data-chart="line"
        data-color="#81c784"
        data-unit="%"
        data-points='{JSON array of [timestamp, value] pairs}'
      ></div>
    </div>

    <div class="data-card chart-card">
      <div class="card-title">💨 Wind Speed</div>
      <div class="mini-chart"
        data-chart="wind"
        data-color="#4fc3f7"
        data-speed='{JSON array of [timestamp, value] pairs}'
        data-direction='{JSON array of [timestamp, degrees] pairs}'
      ></div>
    </div>

  </div>
</div>
```

If no data is found for a location, return:
```html
<div class="placeholder loading">
  <span>No data available for this location yet.</span>
</div>
```

---

## Chart initialization (`/static/charts.js`)

ECharts cannot be initialized by HTMX directly. After each HTMX swap, find all `.mini-chart` elements and initialize them.

```javascript
function initCharts(root) {
  root = root || document;
  root.querySelectorAll('.mini-chart[data-chart]').forEach(el => {
    const dark = window.matchMedia('(prefers-color-scheme: dark)').matches;
    const c = echarts.getInstanceByDom(el) || echarts.init(el, dark ? 'dark' : null, { renderer: 'canvas' });

    if (el.dataset.chart === 'line') {
      const pts = JSON.parse(el.dataset.points || '[]');
      c.setOption(buildLineOption(pts, el.dataset.color, el.dataset.unit), true);

    } else if (el.dataset.chart === 'wind') {
      const speed = JSON.parse(el.dataset.speed || '[]');
      const dir   = JSON.parse(el.dataset.direction || '[]');
      c.setOption(buildWindOption(speed, dir), true);
    }
  });
}

// Run on initial load
document.addEventListener('DOMContentLoaded', () => initCharts());

// Run after every HTMX swap
document.addEventListener('htmx:afterSwap', e => initCharts(e.detail.target));
```

`buildLineOption` and `buildWindOption` implement the same ECharts option shapes as the existing WASM app (see `frontend/src/charts.rs` for the exact JSON).

**Wind direction arrows** — same custom path symbol used in the existing app:
```
path://M 0,-10 L 5,2 L 1,2 L 1,10 L -1,10 L -1,2 L -5,2 Z
```
`symbolRotate = degrees` — points in the meteorological FROM-direction.

---

## Backend SQL queries

```sql
-- Weather: last 24 hours for a station
SELECT obs_name, obs_value, obs_unit, timestamp
FROM weather_observations
WHERE station_code = :station_code
  AND timestamp >= datetime('now', '-1 day')
ORDER BY obs_name, timestamp;

-- Air quality: last 2 hours (latest available reading per pollutant)
SELECT pollutant, value, unit, timestamp
FROM air_quality_observations
WHERE station_name = :station_name
  AND timestamp >= datetime('now', '-2 hours')
ORDER BY pollutant, timestamp;
```

To get `temp_cur`, `humid_cur`, `wind_speed`, `wind_dir` — take the last non-null value per series after grouping by `obs_name`.

To get `temp_max` / `temp_min` with their times — find `MAX(obs_value)` / `MIN(obs_value)` and the corresponding `timestamp` within the "Air Temperature (1.2m)" series.

---

## Degrees → compass conversion

```
directions = ["N","NNE","NE","ENE","E","ESE","SE","SSE","S","SSW","SW","WSW","W","WNW","NW","NNW"]
index = int((degrees + 11.25) / 22.5) % 16
```

---

## Config (`config.toml` — already exists, reuse as-is)

The HTMX backend reads the same `config.toml` used by the WASM backend:
- `[[locations]]` — city name + station mappings
- `[aq_thresholds.{pollutant}]` — `low`, `moderate`, `high` boundaries

---

## Auto-refresh

The outer `<div class="dashboard">` wraps the entire fragment and carries the `hx-trigger="every 5m"` attribute so the whole section auto-refreshes without user interaction.

---

## What changes vs the WASM app

| Aspect | WASM app | HTMX app |
|--------|----------|----------|
| Frontend language | Rust/Leptos compiled to WASM | Plain HTML + HTMX CDN |
| Data format | JSON API → client renders | Server renders HTML directly |
| Charts | Same ECharts (via JS eval) | Same ECharts (via data-* attrs) |
| Auto-refresh | Not implemented | `hx-trigger="every 5m"` |
| Backend change needed | No | No — backend serves HTML instead of JSON |
| Period selector (1d/3d/7d) | Not in UI, hardcoded `1d` | Not required initially |
| Sample/fallback data | Yes (client-side) | Not needed — server can show "—" for missing values |

---

## Backend: C++ + Crow + Inja

| Component | Library | Version | Notes |
|-----------|---------|---------|-------|
| HTTP server | [Crow](https://github.com/CrowCpp/Crow) | v1.2.0 | FetchContent, needs standalone Asio |
| HTML templates | [Inja](https://github.com/pantor/inja) | v3.4.0 | Jinja2-like, uses nlohmann/json |
| Config | toml++ | v3.4.0 | same as collector |
| JSON model | nlohmann/json | v3.11.3 | same as collector |
| Database | SQLite3 | system | same as collector |

Project: `weather-dashboard-cpp/` alongside `weather-collector-cpp/`.

**Run:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/weather-dashboard config.toml   # run from weather-dashboard-cpp/
```

**Template engine:** Inja syntax — `{{ variable }}`, `{% if cond %}`, `{% for x in list %}`.  
Chart data (JSON arrays) is embedded directly in `data-*` attributes via `{{ chart_temp }}`.  
Inja does not HTML-escape by default — double quotes in JSON arrays are safe inside single-quoted attributes.

**Thread safety:** `inja::Environment::render()` is guarded with `std::mutex`; the database connection is opened per-request (SQLite read-only open is cheap).

The only contract:
- `GET /` → full page shell
- `GET /dashboard?location={city}` → HTML fragment as specified above
- `GET /static/*` → CSS + JS files
