# logger

## Purpose

Simple synchronous logger: writes timestamped lines to both a file and standard streams. `INFO` → stdout, `WARN`/`ERROR` → stderr. Used by all collectors to track whether data collection succeeded.

## Key classes / functions

`Logger` — logger class — `src/logger.hpp`

| Method | Action |
|---|---|
| `Logger(path)` | Constructor; takes path to the log file |
| `log.info(msg)` | INFO level → stdout + file |
| `log.warn(msg)` | WARN level → stderr + file |
| `log.error(msg)` | ERROR level → stderr + file |

Line format: `[YYYY-MM-DD HH:MM:SS] LEVEL message`

## Dependencies

Depends on: C++ standard library (`<chrono>`, `<fstream>`, `<format>`)  
Depended on by: `main`

## How to add a feature

**Add a new log level (e.g. DEBUG):**
1. In `src/logger.hpp` add `void debug(std::string_view msg) const`
2. In the body call `write("DEBUG", msg)`
3. In `src/logger.cpp` inside `Logger::write()` adjust stream routing if needed

## How to diagnose a bug

- If the log file is not created — `Logger::write()` prints an error to stderr: `"Logger: cannot write to <path>"`
- The file is opened in `std::ios::app` mode on every `write()` call — no buffering; each message is written immediately
- Timestamps use local time via `localtime_r` — check the system timezone if log times look wrong

## Pitfalls

- `Logger::write()` opens and closes the file on **every call** — no cached file descriptor. Fine for a 30-minute cron job, but inefficient at high call rates
- Uses `localtime_r` (POSIX) instead of `std::chrono::zoned_time` — intentional workaround for an Apple libc++ issue where `zoned_time` requires additional library linkage (comment at `logger.cpp:9`)

## What not to change without discussion

Log line format — if the dashboard or external tools parse the log file, changing the format will break them.
