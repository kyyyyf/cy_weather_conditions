#include "air_quality_collector.hpp"
#include "http.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <format>
#include <ranges>
#include <set>                  // portable fallback
#if __has_include(<flat_set>)
#  include <flat_set>
   template<class K> using FlatSet = std::flat_set<K>;  // GCC 14+, libc++ 18+
#else
   template<class K> using FlatSet = std::set<K>;       // Apple Clang fallback
#endif

namespace {

constexpr std::string_view kApiBase =
    "https://www.airquality.dli.mlsi.gov.cy";

// ── Trim ──────────────────────────────────────────────────────────────────────

constexpr std::string_view trim_sv(std::string_view s) noexcept {
    const auto not_space = [](unsigned char c){ return !std::isspace(c); };
    const auto b = std::ranges::find_if(s, not_space);
    if (b == s.end()) return {};
    const auto e = std::ranges::find_if(s | std::views::reverse, not_space);
    return s.substr(
        static_cast<std::size_t>(b - s.begin()),
        s.size() - static_cast<std::size_t>(e - s.rbegin()));
}

// ── Name & pollutant normalisation ────────────────────────────────────────────

// Lowercase + collapse whitespace for fuzzy station name matching.
std::string normalise_name(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    bool prev_space = false;
    for (const unsigned char c : s) {
        if (std::isspace(c)) {
            if (!out.empty()) prev_space = true;
        } else {
            if (prev_space) { out += ' '; prev_space = false; }
            out += static_cast<char>(std::tolower(c));
        }
    }
    return out;
}

// Replaces unicode subscript digits (U+2080–U+2089 = UTF-8 E2 82 8x) with ASCII.
// "SO₂" → "SO2",  "PM₁₀" → "PM10",  "C₆H₆" → "C6H6"
std::string normalise_pollutant(std::string_view s) {
    s = trim_sv(s);
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ) {
        const auto b0 = static_cast<unsigned char>(s[i]);
        if (b0 == 0xE2 && i + 2 < s.size()
            && static_cast<unsigned char>(s[i+1]) == 0x82
            && static_cast<unsigned char>(s[i+2]) >= 0x80
            && static_cast<unsigned char>(s[i+2]) <= 0x89)
        {
            out += static_cast<char>('0' + (static_cast<unsigned char>(s[i+2]) - 0x80));
            i += 3;
        } else {
            out += s[i++];
        }
    }
    return out;
}

// CO → "mg/m³", everything else → "μg/m³"
constexpr std::string_view standard_unit(std::string_view pollutant) noexcept {
    return (pollutant == "CO") ? "mg/m³" : "μg/m³";
}

std::optional<double> parse_double(std::string_view s) {
    s = trim_sv(s);
    if (s.empty()) return std::nullopt;
    double v{};
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec == std::errc{} && ptr == s.data() + s.size()) return v;
    return std::nullopt;
}

// ── Time window ───────────────────────────────────────────────────────────────

// Returns {from, to} as URL-encoded "YYYY-MM-DD%20HH:00" strings.
// Uses POSIX localtime_r to avoid Apple libc++ timezone linkage issues.
std::pair<std::string, std::string> build_time_window() {
    using namespace std::chrono;
    const auto now  = floor<hours>(system_clock::now());
    const auto from = now - hours{2};

    const auto fmt = [](system_clock::time_point tp) {
        const auto tt = system_clock::to_time_t(tp);
        std::tm tm{};
        ::localtime_r(&tt, &tm);
        return std::format("{:04}-{:02}-{:02}%20{:02}:00",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour);
    };
    return {fmt(from), fmt(now)};
}

// ── JSON parsing ──────────────────────────────────────────────────────────────

std::expected<std::vector<StationReading>, std::string>
parse_aq_json(const nlohmann::json& data, std::span<const std::string> names) {
    // flat_set: sorted contiguous storage, cache-friendly for our small station list
    const auto norm_names = names
        | std::views::transform([](const std::string& n){ return normalise_name(n); })
        | std::ranges::to<FlatSet<std::string>>();

    std::vector<StationReading> results;

    for (const auto& [_, station] : data.items()) {        // C++26: _ placeholder
        if (!station.is_object()) continue;

        const auto name_en = std::string{trim_sv(station.value("name_en", std::string{}))};
        if (!norm_names.contains(normalise_name(name_en))) continue;

        const auto values = station.find("values");
        if (values == station.end() || !values->is_object()) continue;

        for (const auto& [_, hourly] : values->items()) {  // C++26: _ reuse
            if (!hourly.is_object()) continue;

            const auto polls = hourly.find("pollutants");
            if (polls == hourly.end() || !polls->is_object()) continue;

            auto ts = polls->value("date_time", std::string{});
            if (ts.size() > 16) ts.resize(16);
            if (ts.size() < 16) continue;  // malformed — skip

            std::vector<Measurement> measurements;
            for (const auto& [pkey, pval] : polls->items()) {
                if (!pkey.starts_with("pollutant_") || !pval.is_object()) continue;

                auto pollutant = normalise_pollutant(pval.value("notation", std::string{}));
                if (pollutant.empty()) continue;

                // Compute unit BEFORE moving pollutant into the struct
                auto unit = std::string{standard_unit(pollutant)};
                measurements.push_back(Measurement{
                    .pollutant = std::move(pollutant),
                    .value     = parse_double(pval.value("value", std::string{})),
                    .unit      = std::move(unit),
                });
            }

            if (!measurements.empty()) {
                results.push_back(StationReading{
                    .station_name = name_en,
                    .timestamp    = std::move(ts),
                    .measurements = std::move(measurements),
                });
            }
        }
    }
    return results;
}

} // namespace

// ── Public API ────────────────────────────────────────────────────────────────

std::expected<std::vector<StationReading>, std::string>
fetch_air_quality(std::span<const std::string> names) {
    auto [from, to]  = build_time_window();
    const auto url   = std::format("{}/all_stations_data_range_PM/{}/{}", kApiBase, from, to);

    return http_get(url, "weather-collector-cpp/0.1")
        .and_then([](HttpResponse r) -> std::expected<nlohmann::json, std::string> {
            if (r.status_code != 200)
                return std::unexpected(std::format("AQ API returned HTTP {}", r.status_code));
            try   { return nlohmann::json::parse(r.body); }
            catch (const nlohmann::json::exception& e) {
                return std::unexpected(std::format("JSON parse error: {}", e.what()));
            }
        })
        .and_then([&names](nlohmann::json body) -> std::expected<std::vector<StationReading>, std::string> {
            if (body.value("status", 0) != 1)
                return std::unexpected(
                    std::format("AQ API error: {}", body.value("msg", std::string{"unknown"})));
            const auto it = body.find("data");
            if (it == body.end() || !it->is_object())
                return std::unexpected(std::string{"'data' field missing in AQ API response"});
            return parse_aq_json(*it, names);
        });
}
