#include "Logger.hpp"

#include <iostream>

namespace core
{
    void Logger::init()
    {
        info("Logger initialized");
    }

    void Logger::info(const std::string& message)
    {
        write(LogLevel::Info, message);
    }

    void Logger::warning(const std::string& message)
    {
        write(LogLevel::Warning, message);
    }

    void Logger::error(const std::string& message)
    {
        write(LogLevel::Error, message);
    }

    void Logger::debug(const std::string& message)
    {
        write(LogLevel::Debug, message);
    }

    void Logger::write(LogLevel level, const std::string& message)
    {
        const char* prefix = "[INFO]";

        switch (level)
        {
            case LogLevel::Info:    prefix = "[INFO]"; break;
            case LogLevel::Warning: prefix = "[WARN]"; break;
            case LogLevel::Error:   prefix = "[ERR ]"; break;
            case LogLevel::Debug:   prefix = "[DBG ]"; break;
        }

        std::cout << prefix << " " << message << std::endl;
    }
}