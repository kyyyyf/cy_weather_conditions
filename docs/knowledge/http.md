# http

## Purpose

Thin wrapper around libcurl for synchronous HTTP GET requests. The sole networking layer in the project — both collectors use only this module.

## Key classes / functions

`HttpResponse` — response struct — `src/http.hpp`

| Field | Type | Description |
|---|---|---|
| `status_code` | `int` | HTTP status (200, 404, …) |
| `body` | `std::string` | Response body |

`http_get(url, user_agent)` — performs a GET request, returns `std::expected<HttpResponse, std::string>` — `src/http.hpp`

- `url` — `std::string_view`, full URL
- `user_agent` — optional, defaults to `"weather-collector-cpp/0.1"`
- Network error → `unexpected(curl_error_string)`
- HTTP 4xx/5xx → **not** an error at the `http_get` level; status is passed through in `HttpResponse.status_code` and checked by each collector

## Dependencies

Depends on: libcurl (system library)  
Depended on by: `dom_collector`, `air_quality_collector`

## How to add a feature

**Add request header support:**
1. Extend the `http_get` signature in `src/http.hpp`
2. In `src/http.cpp` pass headers via `curl_slist` before `curl_easy_perform`

**Add a new data source** — a new collector calls `http_get` directly; nothing in `http.*` needs to change.

## How to diagnose a bug

- `http_get` returned `unexpected` → network error (DNS, timeout, TLS). The text is a libcurl error string
- `status_code != 200` on a successful return → server returned an error; collectors log this as `"DOM XML returned HTTP {}"` or `"AQ API returned HTTP {}"`
- Check that the process has network access (relevant in containers)

## Pitfalls

- `src/http.cpp` implementation details (timeout, SSL verification, redirect behaviour) are not exposed by the header — read the source file if these matter
- No support for POST, request headers, or authentication — all would need to be added in `http.cpp`

## What not to change without discussion

Signature of `http_get` — collectors call it directly inside `.and_then()` chains.
