cmake_minimum_required(VERSION 3.26)
project(weather-collector-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(tomlplusplus nlohmann_json pugixml)

find_package(CURL    REQUIRED)
find_package(SQLite3 REQUIRED)

# ── Target ────────────────────────────────────────────────────────────────────
add_executable(weather-collector
    src/main.cpp
    src/config.cpp
    src/logger.cpp
    src/database.cpp
    src/http.cpp
    src/dom_collector.cpp
    src/air_quality_collector.cpp
)

target_include_directories(weather-collector PRIVATE src)

target_link_libraries(weather-collector PRIVATE
    CURL::libcurl
    SQLite3::SQLite3
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    pugixml::pugixml
)

# ── Compile options ───────────────────────────────────────────────────────────
target_compile_options(weather-collector PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wnull-dereference
        -Wno-unused-parameter
        $<$<CONFIG:Release>: -O3 -march=native>
        $<$<CONFIG:Debug>:   -O0 -g
            -fsanitize=address,undefined
            -fno-omit-frame-pointer>
    >
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>: -O3 -march=native>
        $<$<CONFIG:Debug>:   -O0 -g -fsanitize=address,undefined>
    >
    $<$<CXX_COMPILER_ID:MSVC>: /W4 /utf-8>
)

target_link_options(weather-collector PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Debug>>:
        -fsanitize=address,undefined>
)

# LTO for release builds
set_target_properties(weather-collector PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
)

message(STATUS "Compiler : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ std  : ${CMAKE_CXX_STANDARD}")
message(STATUS "Build    : ${CMAKE_BUILD_TYPE}")
