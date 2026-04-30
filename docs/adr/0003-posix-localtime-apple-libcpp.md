# ADR-0003: POSIX localtime_r instead of std::chrono::zoned_time

**Status:** Accepted  
**Date:** 2026-04-29

## Context

Two files (`logger.cpp:9`, `air_quality_collector.cpp:101`) contain an explicit comment:

```cpp
// POSIX localtime_r: reliable local time on any platform, no timezone
// library linkage required (unlike std::chrono::zoned_time on Apple libc++).
```

`std::chrono::zoned_time` (C++20) on Apple libc++ requires additional linkage with the timezone database library, which complicates macOS builds.

## Decision

Use `localtime_r` (POSIX) to obtain local time — available without extra dependencies on all target platforms (macOS, Linux).

## Consequences

- `localtime_r` is POSIX, not standard C++ — not available on MSVC/Windows without additional wrappers
- Porting to Windows would require replacing it with `localtime_s` or `std::chrono::zoned_time` with explicit linking
- New code that needs local time should follow the same pattern — do not use `zoned_time` while the project targets macOS
