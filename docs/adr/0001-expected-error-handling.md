# ADR-0001: std::expected for error handling instead of exceptions

**Status:** Accepted  
**Date:** 2026-04-29

## Context

All public functions in the project (`Config::load`, `Database::open`, `Database::insert_*`, `http_get`, `fetch_weather`, `fetch_air_quality`) return `std::expected<T, std::string>` and are marked `[[nodiscard]]`. No exceptions are thrown (third-party library exceptions are immediately caught and converted to `unexpected`).

`main.cpp` and collectors use `.and_then()` chains instead of `try/catch`.

## Decision

Use `std::expected<T, std::string>` (C++23) as the sole error-propagation mechanism. The error string is human-readable text ready for logging.

## Consequences

- All callers must check the result (`[[nodiscard]]` produces a warning otherwise)
- New functions must follow the same pattern: no exceptions, return `std::expected`
- Third-party exceptions (toml++, nlohmann/json, pugixml) are wrapped in `try/catch` at the call site and converted to `unexpected` — see `config.cpp:8`, `air_quality_collector.cpp:179`
