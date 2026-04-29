#include "http.hpp"
#include <curl/curl.h>
#include <format>

namespace {

// libcurl write callback — appends received data to a std::string
std::size_t write_cb(char* ptr, std::size_t /*size*/, std::size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, nmemb);
    return nmemb;
}

// RAII wrapper for CURL*
struct CurlHandle {
    CURL* ptr;
    explicit CurlHandle() : ptr(curl_easy_init()) {}
    ~CurlHandle() { if (ptr) curl_easy_cleanup(ptr); }
    CurlHandle(const CurlHandle&)            = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;
    explicit operator bool() const { return ptr != nullptr; }
};

// One-time global init/cleanup
struct CurlGlobal {
    CurlGlobal()  { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

} // namespace

std::expected<HttpResponse, std::string> http_get(
    std::string_view url,
    std::string_view user_agent)
{
    static CurlGlobal global_init;

    CurlHandle curl;
    if (!curl) {
        return std::unexpected("curl_easy_init() failed");
    }

    std::string body;
    body.reserve(64 * 1024);

    std::string url_str{url};
    std::string ua_str{user_agent};

    curl_easy_setopt(curl.ptr, CURLOPT_URL,           url_str.c_str());
    curl_easy_setopt(curl.ptr, CURLOPT_USERAGENT,     ua_str.c_str());
    curl_easy_setopt(curl.ptr, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl.ptr, CURLOPT_WRITEDATA,     &body);
    curl_easy_setopt(curl.ptr, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.ptr, CURLOPT_TIMEOUT,        30L);
    // TLS verification on by default (libcurl uses system CA bundle)

    CURLcode res = curl_easy_perform(curl.ptr);
    if (res != CURLE_OK) {
        return std::unexpected(
            std::format("HTTP request failed: {}", curl_easy_strerror(res)));
    }

    long status = 0;
    curl_easy_getinfo(curl.ptr, CURLINFO_RESPONSE_CODE, &status);

    return HttpResponse{static_cast<int>(status), std::move(body)};
}
