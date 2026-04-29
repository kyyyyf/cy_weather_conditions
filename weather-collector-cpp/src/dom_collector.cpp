#include "dom_collector.hpp"
#include "http.hpp"
#include <pugixml.hpp>
#include <charconv>
#include <format>
#include <ranges>
#include <set>                  // portable fallback
#if __has_include(<flat_set>)
#  include <flat_set>
   template<class K> using StationSet = std::flat_set<K>;  // GCC 14+, libc++ 18+
#else
   template<class K> using StationSet = std::set<K>;       // Apple Clang fallback
#endif

namespace {

constexpr std::string_view kDomUrl =
    "https://www.dom.org.cy/AWS/OpenData/CyDoM.xml";

// Trim leading/trailing whitespace from a string_view.
constexpr std::string_view trim_sv(std::string_view s) noexcept {
    auto not_space = [](unsigned char c){ return !std::isspace(c); };
    auto b = std::ranges::find_if(s, not_space);
    auto e = std::ranges::find_if(s | std::views::reverse, not_space);
    if (b == s.end()) return {};
    return s.substr(
        static_cast<std::size_t>(b - s.begin()),
        s.size() - static_cast<std::size_t>(e - s.rbegin()));
}

// "2026-04-13 14:20 (Local Time)"  →  "2026-04-13 14:20"
std::string normalise_dom_timestamp(std::string_view s) {
    if (auto pos = s.find(" ("); pos != std::string_view::npos)
        s = s.substr(0, pos);
    return std::string{trim_sv(s)};
}

std::optional<double> parse_double(std::string_view s) {
    s = trim_sv(s);
    if (s.empty()) return std::nullopt;
    double v{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec == std::errc{} && ptr == s.data() + s.size()) return v;
    return std::nullopt;
}

// Parses the DOM XML string and returns data for the requested station codes.
std::expected<std::vector<StationData>, std::string>
parse_xml(const std::string& xml, std::span<const std::string> codes) {
    // std::flat_set: sorted contiguous storage, cache-friendly for small sets
    const StationSet<std::string> wanted(codes.begin(), codes.end());

    pugi::xml_document doc;
    if (auto pr = doc.load_string(xml.c_str()); !pr)
        return std::unexpected(std::format("XML parse error: {}", pr.description()));

    std::vector<StationData> results;

    // <observations> blocks are direct children of the document root element.
    // Direct iteration is O(n) and avoids XPath overhead.
    for (pugi::xml_node node : doc.first_child().children("observations")) {
        const auto code = std::string{trim_sv(node.child_value("station_code"))};
        if (!wanted.contains(code)) continue;

        StationData sd{
            .station_code = code,
            .timestamp    = normalise_dom_timestamp(node.child_value("date_time")),
        };

        for (pugi::xml_node obs : node.children("observation")) {
            const std::string name = obs.child_value("observation_name");
            if (name.empty()) continue;
            sd.observations.push_back(Observation{
                .name  = name,
                .value = parse_double(obs.child_value("observation_value")),
                .unit  = obs.child_value("observation_unit"),
            });
        }

        results.push_back(std::move(sd));
    }
    return results;
}

} // namespace

// ── Public API ────────────────────────────────────────────────────────────────

std::expected<std::vector<StationData>, std::string>
fetch_weather(std::span<const std::string> codes) {
    return http_get(kDomUrl)
        .and_then([](HttpResponse r) -> std::expected<std::string, std::string> {
            if (r.status_code != 200)
                return std::unexpected(std::format("DOM XML returned HTTP {}", r.status_code));
            return std::move(r.body);
        })
        .and_then([&codes](std::string body) {
            return parse_xml(body, codes);
        });
}
