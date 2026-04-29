#include "logger.hpp"
#include <chrono>
#include <ctime>
#include <format>
#include <fstream>
#include <print>

void Logger::write(std::string_view level, std::string_view msg) const {
    // POSIX localtime_r: reliable local time on any platform, no timezone
    // library linkage required (unlike std::chrono::zoned_time on Apple libc++).
    using namespace std::chrono;
    const auto now_s = floor<seconds>(system_clock::now());
    const auto tt    = system_clock::to_time_t(now_s);
    std::tm tm{};
    ::localtime_r(&tt, &tm);

    const auto ts = std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    const auto line = std::format("[{}] {:<5} {}", ts, level, msg);

    // INFO → stdout, WARN/ERROR → stderr  (mirrors Rust logger behaviour)
    std::println(level == "INFO" ? stdout : stderr, "{}", line);

    if (std::ofstream f{path_, std::ios::app}) {
        f << line << '\n';
    } else {
        std::println(stderr, "Logger: cannot write to {}", path_.string());
    }
}
