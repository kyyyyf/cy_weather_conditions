# ── Stage 1: build ───────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake ninja-build \
        gcc-14 g++-14 \
        libsqlite3-dev \
        git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc-14 \
        -DCMAKE_CXX_COMPILER=g++-14 \
    && cmake --build build

# ── Stage 2: runtime ─────────────────────────────────────────────────────────
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        libsqlite3-0 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src/build/weather-dashboard .
COPY templates/ templates/
COPY static/    static/

EXPOSE 3000

ENTRYPOINT ["./weather-dashboard"]
CMD ["config.toml"]
