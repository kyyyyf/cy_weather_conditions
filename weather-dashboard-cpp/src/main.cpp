#include "config.hpp"
#include "database.hpp"

#include <thread>  // must precede crow.h on Apple Clang (std::this_thread::yield)
#include <crow.h>
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <fstream>
#include <mutex>
#include <print>
#include <ranges>
#include <string>

using json = nlohmann::json;

// ── AQ classification ────────────────────────────────────────────────────────

static int classify_level(double v, const AqBand& b) {
    if (v < b.low)      return 0; // Good
    if (v < b.moderate) return 1; // Moderate
    if (v < b.high)     return 2; // High
    return 3;                      // Very High
}

static constexpr std::string_view kLevelLabel[] = {"Good","Moderate","High","Very High"};
static constexpr std::string_view kLevelColor[] = {"#4caf50","#ffc107","#ff5722","#f44336"};

static int aq_sort_key(const std::string& name) {
    static const std::unordered_map<std::string,int> kOrder = {
        {"PM10",0},{"PM2.5",1},{"O3",2},{"NO2",3},
        {"NOx",4},{"NO",5},{"SO2",6},{"CO",7},{"C6H6",8}
    };
    auto it = kOrder.find(name);
    return it != kOrder.end() ? it->second : 99;
}

// ── Data helpers ─────────────────────────────────────────────────────────────

static const TimeSeries* find_series(
    const std::vector<TimeSeries>& series,
    std::string_view kw,
    std::string_view kw2 = {})
{
    auto lower = [](std::string s) {
        std::ranges::transform(s, s.begin(), [](unsigned char c){ return std::tolower(c); });
        return s;
    };
    const auto k1 = lower(std::string{kw});
    const auto k2 = lower(std::string{kw2});
    for (const auto& s : series) {
        const auto n = lower(s.name);
        if (!n.contains(k1)) continue;
        if (!k2.empty() && !n.contains(k2)) continue;
        return &s;
    }
    return nullptr;
}

static std::optional<double> last_val(const TimeSeries* s) {
    if (!s) return std::nullopt;
    for (auto it = s->data.rbegin(); it != s->data.rend(); ++it)
        if (it->value) return it->value;
    return std::nullopt;
}

static json chart_pts(const TimeSeries* s) {
    auto arr = json::array();
    if (!s) return arr;
    for (const auto& pt : s->data)
        if (pt.value)
            arr.push_back({pt.timestamp, *pt.value});
    return arr;
}

static std::string_view degrees_to_compass(double deg) {
    static constexpr std::string_view kDirs[] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    return kDirs[static_cast<size_t>((deg + 11.25) / 22.5) % 16];
}

static std::string fmt_val(std::optional<double> v, const char* suffix = "") {
    return v ? std::format("{:.1f}{}", *v, suffix) : "—";
}

// ── Dashboard data builder ────────────────────────────────────────────────────

static std::expected<json, std::string>
build_dashboard_data(const Config& cfg, const std::string& location_name) {
    // Look up location
    auto loc_it = std::ranges::find_if(cfg.locations,
        [&](const LocationInfo& l){ return l.name == location_name; });
    if (loc_it == cfg.locations.end())
        return std::unexpected("Unknown location: " + location_name);

    // Open DB per-request (read-only SQLite is cheap to open)
    auto db = Database::open(cfg.database_path);
    if (!db) return std::unexpected(db.error());

    auto weather_res = db->fetch_weather(loc_it->weather_code, 1);
    if (!weather_res) return std::unexpected(weather_res.error());

    auto aq_res = db->fetch_air_quality(loc_it->aq_station);
    if (!aq_res) return std::unexpected(aq_res.error());

    const auto& series     = *weather_res;
    const auto& aq_readings = *aq_res;

    // ── Find series
    const auto* temp_s  = find_series(series, "temperature");
    const auto* humid_s = find_series(series, "humidity");
    const auto* speed_s = find_series(series, "wind", "speed");
    const auto* dir_s   = find_series(series, "wind", "dir");

    // ── Temperature stats
    const auto temp_cur = last_val(temp_s);
    std::optional<std::pair<double,std::string>> temp_max, temp_min;
    if (temp_s) {
        for (const auto& pt : temp_s->data) {
            if (!pt.value) continue;
            const auto t = pt.timestamp.size() >= 16
                ? pt.timestamp.substr(11, 5) : pt.timestamp;
            if (!temp_max || *pt.value > temp_max->first) temp_max = {*pt.value, t};
            if (!temp_min || *pt.value < temp_min->first) temp_min = {*pt.value, t};
        }
    }

    // ── Wind
    const auto wind_dir   = last_val(dir_s);
    const auto compass    = wind_dir
        ? std::string{degrees_to_compass(*wind_dir)} : "—";
    const auto wind_speed = last_val(speed_s);
    const auto wind_unit  = speed_s ? speed_s->unit : std::string{"m/s"};

    // ── AQ stats
    int worst_level = -1;
    std::optional<double> pm10_val, pm25_val;
    int pm10_level = 0, pm25_level = 0;

    for (const auto& r : aq_readings) {
        if (!r.value) continue;
        auto thr_it = cfg.aq_thresholds.find(r.pollutant);
        int lvl = (thr_it != cfg.aq_thresholds.end())
            ? classify_level(*r.value, thr_it->second) : 0;
        worst_level = std::max(worst_level, lvl);
        if (r.pollutant == "PM10")  { pm10_val = r.value; pm10_level = lvl; }
        if (r.pollutant == "PM2.5") { pm25_val = r.value; pm25_level = lvl; }
    }

    // ── AQ table rows (sorted)
    auto sorted_aq = aq_readings;
    std::sort(sorted_aq.begin(), sorted_aq.end(),
        [](const AqReading& a, const AqReading& b){
            return aq_sort_key(a.pollutant) < aq_sort_key(b.pollutant);
        });

    json aq_rows = json::array();
    for (const auto& r : sorted_aq) {
        auto thr_it = cfg.aq_thresholds.find(r.pollutant);
        int lvl = (r.value && thr_it != cfg.aq_thresholds.end())
            ? classify_level(*r.value, thr_it->second) : -1;
        aq_rows.push_back({
            {"name",  r.pollutant},
            {"val",   r.value ? std::format("{:.1f}", *r.value) : "—"},
            {"unit",  r.unit},
            {"label", lvl >= 0 ? std::string{kLevelLabel[lvl]} : "—"},
            {"color", lvl >= 0 ? std::string{kLevelColor[lvl]} : "#666"},
        });
    }

    // ── Chart data
    const auto ctemp  = chart_pts(temp_s);
    const auto chumid = chart_pts(humid_s);
    const auto cspeed = chart_pts(speed_s);
    const auto cdir   = chart_pts(dir_s);

    // ── Assemble template data
    json d;
    d["location"]      = location_name;

    // Temperature
    d["temp_cur"]      = fmt_val(temp_cur, "°C");
    d["has_temp_max"]  = temp_max.has_value();
    d["temp_max_val"]  = temp_max ? std::format("{:.1f}°C", temp_max->first)  : "—";
    d["temp_max_time"] = temp_max ? temp_max->second : "";
    d["has_temp_min"]  = temp_min.has_value();
    d["temp_min_val"]  = temp_min ? std::format("{:.1f}°C", temp_min->first)  : "—";
    d["temp_min_time"] = temp_min ? temp_min->second : "";

    // Humidity & wind
    d["humid_cur"]     = fmt_val(last_val(humid_s), "%");
    d["compass"]       = compass;
    d["wind_speed_str"]= wind_speed
        ? std::format("{:.1f} {}", *wind_speed, wind_unit) : "—";

    // AQ summary
    const bool has_aq = !aq_readings.empty();
    d["has_aq"]           = has_aq;
    d["worst_aq_label"]   = worst_level >= 0 ? std::string{kLevelLabel[worst_level]} : "N/A";
    d["worst_aq_color"]   = worst_level >= 0 ? std::string{kLevelColor[worst_level]} : "#555";
    d["has_pm10"]         = pm10_val.has_value();
    d["pm10_val"]         = pm10_val ? std::format("{:.1f}", *pm10_val) : "—";
    d["pm10_color"]       = std::string{kLevelColor[pm10_level]};
    d["has_pm25"]         = pm25_val.has_value();
    d["pm25_val"]         = pm25_val ? std::format("{:.1f}", *pm25_val) : "—";
    d["pm25_color"]       = std::string{kLevelColor[pm25_level]};
    d["aq_rows"]          = std::move(aq_rows);

    // Charts
    d["has_chart_temp"]  = !ctemp.empty();
    d["chart_temp"]      = ctemp;
    d["has_chart_humid"] = !chumid.empty();
    d["chart_humid"]     = chumid;
    d["has_chart_wind"]  = !cspeed.empty();
    d["chart_wind_speed"]= cspeed;
    d["chart_wind_dir"]  = cdir;

    return d;
}

// ── Static file helper ────────────────────────────────────────────────────────

static crow::response serve_static(const std::string& fpath) {
    if (fpath.find("..") != std::string::npos)
        return crow::response{403};

    std::ifstream f{"./static/" + fpath, std::ios::binary};
    if (!f) return crow::response{404};

    crow::response res;
    res.body.assign(std::istreambuf_iterator<char>{f}, {});

    if      (fpath.ends_with(".css")) res.set_header("Content-Type", "text/css; charset=utf-8");
    else if (fpath.ends_with(".js"))  res.set_header("Content-Type", "application/javascript; charset=utf-8");
    res.code = 200;
    return res;
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    const std::string cfg_path = (argc > 1) ? argv[1] : "config.toml";

    auto cfg_res = Config::load(cfg_path);
    if (!cfg_res) { std::println(stderr, "Error: {}", cfg_res.error()); return 1; }
    Config cfg = std::move(*cfg_res);

    // Pre-parse templates once at startup
    inja::Environment env{"./templates/"};
    auto index_tmpl = env.parse_template("index.html");
    auto dash_tmpl  = env.parse_template("dashboard.html");

    // Mutex guards inja::Environment::render (thread safety not guaranteed)
    std::mutex render_mtx;

    crow::SimpleApp app;

    // ── GET / ─────────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/")(
        [&]() -> crow::response {
            json data;
            data["locations"] = json::array();
            for (const auto& loc : cfg.locations)
                data["locations"].push_back(loc.name);

            std::string html;
            {
                std::lock_guard lock{render_mtx};
                html = env.render(index_tmpl, data);
            }
            crow::response res{html};
            res.set_header("Content-Type", "text/html; charset=utf-8");
            return res;
        }
    );

    // ── GET /dashboard?location=... ────────────────────────────────────────────
    CROW_ROUTE(app, "/dashboard")(
        [&](const crow::request& req) -> crow::response {
            const char* loc_raw = req.url_params.get("location");
            if (!loc_raw || std::string_view{loc_raw}.empty())
                return crow::response{400};

            auto data_res = build_dashboard_data(cfg, std::string{loc_raw});

            std::string html;
            if (!data_res) {
                html = std::format(
                    "<div class='placeholder'><span>Error: {}</span></div>",
                    data_res.error());
            } else {
                std::lock_guard lock{render_mtx};
                html = env.render(dash_tmpl, *data_res);
            }

            crow::response res{html};
            res.set_header("Content-Type", "text/html; charset=utf-8");
            return res;
        }
    );

    // ── GET /static/<file> ────────────────────────────────────────────────────
    CROW_ROUTE(app, "/static/<path>")(
        [](const std::string& fpath) { return serve_static(fpath); }
    );

    std::println("Dashboard running on http://0.0.0.0:{}", cfg.port);
    app.port(cfg.port).run();
}
