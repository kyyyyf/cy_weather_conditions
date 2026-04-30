# ADR-0002: INSERT OR IGNORE for write idempotency

**Status:** Accepted  
**Date:** 2026-04-29

## Context

Both insert methods in `database.cpp` use `INSERT OR IGNORE`:

```cpp
"INSERT OR IGNORE INTO weather_observations ..."
"INSERT OR IGNORE INTO air_quality_observations ..."
```

Both tables have a `UNIQUE` constraint on `(station_code/name, timestamp, obs_name/pollutant)`.

The README explicitly documents: *"Both use INSERT OR IGNORE — safe to run concurrently with the dashboard reader."*

## Decision

The collector runs on a cron schedule. If it is re-run within the same hour, the data is already in the DB. `INSERT OR IGNORE` ensures a duplicate is silently skipped without returning an error. This makes the collector safe to re-run without any "already written?" check.

## Consequences

- Duplicate inserts are a normal situation, not an error; `insert_*()` returns `Ok` even for ignored rows
- Do not use `INSERT OR REPLACE` / `UPDATE` without an explicit decision — this changes the contract and could cause data loss on accidental re-runs with a different source
- New tables should follow the same pattern: `UNIQUE` constraint + `INSERT OR IGNORE`
