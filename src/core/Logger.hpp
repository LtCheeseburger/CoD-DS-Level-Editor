#pragma once

#include <string>

namespace core
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error,
        Debug
    };

    class Logger
    {
    public:
        static void init();

        static void info(const std::string& message);
        static void warning(const std::string& message);
        static void error(const std::string& message);
        static void debug(const std::string& message);

    private:
        static void write(LogLevel level, const std::string& message);
    };
}