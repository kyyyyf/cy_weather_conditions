cmake_minimum_required(VERSION 3.26)
project(weather-dashboard-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ── Asio (standalone, required by Crow) ──────────────────────────────────────
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)
# Tell Crow where asio.hpp lives before it runs its own find_path()
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "" FORCE)

# ── Crow ─────────────────────────────────────────────────────────────────────
FetchContent_Declare(crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        v1.2.0
    GIT_SHALLOW    TRUE
)
set(CROW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(CROW_BUILD_TESTS    OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(crow)

# ── Inja (Jinja2-like templates, uses nlohmann/json internally) ───────────────
FetchContent_Declare(inja
    GIT_REPOSITORY https://github.com/pantor/inja.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
set(INJA_BUILD_TESTS OFF CACHE INTERNAL "")
set(INJA_EXPORT      OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(inja)

# ── toml++ ───────────────────────────────────────────────────────────────────
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
# ── nlohmann/json (also pulled transitively by inja) ─────────────────────────
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(tomlplusplus nlohmann_json)

find_package(SQLite3  REQUIRED)
find_package(Threads  REQUIRED)

# ── Target ───────────────────────────────────────────────────────────────────
add_executable(weather-dashboard
    src/main.cpp
    src/config.cpp
    src/database.cpp
)

set_target_properties(weather-dashboard PROPERTIES
    CXX_STANDARD          23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

target_include_directories(weather-dashboard PRIVATE src)

target_link_libraries(weather-dashboard PRIVATE
    Crow::Crow
    pantor::inja
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    SQLite3::SQLite3
    Threads::Threads
)

target_compile_options(weather-dashboard PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall -Wextra -Wno-unused-parameter
        $<$<CONFIG:Release>: -O3 -march=native>
        $<$<CONFIG:Debug>:   -O0 -g>
    >
)

message(STATUS "Compiler : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build    : ${CMAKE_BUILD_TYPE}")
