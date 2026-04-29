#pragma once
#include <filesystem>
#include <string_view>

class Logger {
    std::filesystem::path path_;

    void write(std::string_view level, std::string_view msg) const;

public:
    explicit Logger(std::filesystem::path path) : path_(std::move(path)) {}

    void info (std::string_view msg) const { write("INFO",  msg); }
    void warn (std::string_view msg) const { write("WARN",  msg); }
    void error(std::string_view msg) const { write("ERROR", msg); }
};
