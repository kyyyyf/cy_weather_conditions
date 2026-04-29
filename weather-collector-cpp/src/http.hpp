#pragma once
#include <expected>
#include <string>
#include <string_view>

struct HttpResponse {
    int         status_code;
    std::string body;
};

/// Synchronous HTTP GET. Returns the response or an error string.
[[nodiscard]]
std::expected<HttpResponse, std::string> http_get(
    std::string_view url,
    std::string_view user_agent = "weather-collector-cpp/0.1"
);
