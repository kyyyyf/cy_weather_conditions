# ADR-0004: Docker named volume for SQLite sharing between collector and web app

**Status:** Accepted  
**Date:** 2026-04-30

## Context

The collector and the web app are separate projects deployed on the same VPS. Both need access to the same SQLite file. Options considered:

1. **Bind mount to a host path** — both stacks mount the same host directory (e.g. `/opt/cy_weather/db`). Simple, but couples both stacks to a specific host path agreed upon out-of-band.
2. **Named Docker volume** — collector stack creates `cy_weather_db`; web app stack references it as `external: true`. No host path coordination required.
3. **Single shared `docker-compose.yml`** — both services in one file. Rejected: the web app is a separate project with its own release cycle.

## Decision

Use a named Docker volume (`cy_weather_db`). The collector stack owns the volume definition; the web app stack declares it as `external: true`. The web app mounts the volume read-only.

## Consequences

- The collector stack must be deployed and `docker compose up` run at least once before the web app stack starts, so the volume exists
- The volume name `cy_weather_db` is a shared contract between the two projects — renaming it requires coordinated change in both repos
- SQLite file path inside containers: `/data/weather_conditions.db` — this is the `DB_PATH` the web app should read from
- No WAL mode is enabled in the collector; brief read/write contention is possible but acceptable for a 30-minute cron interval
