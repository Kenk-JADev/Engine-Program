#include "rpgmaker3d/Logger.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace rpg {

Logger& Logger::Get() {
    static Logger instance;
    return instance;
}

void Logger::SetLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mMutex);
    mLogFile = path;
}

void Logger::SetConsoleOutput(bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    mConsoleOutput = enabled;
}

void Logger::SetMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mMutex);
    mMinLevel = level;
}

const char* Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

void Logger::Log(LogLevel level, const std::string& message, const std::string& source) {
    if (level < mMinLevel) return;

    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float time = std::chrono::duration<float>(now - start).count();

    LogEntry entry{level, message, source, time};

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEntries.push_back(entry);
        if (mEntries.size() > 10000) {
            mEntries.erase(mEntries.begin(), mEntries.begin() + 1000);
        }
    }

    if (mConsoleOutput) WriteToConsole(entry);
    WriteToFile(entry);

    if (mCallback) mCallback(entry);
}

void Logger::Trace(const std::string& msg, const std::string& source)   { Log(LogLevel::Trace, msg, source); }
void Logger::Debug(const std::string& msg, const std::string& source)   { Log(LogLevel::Debug, msg, source); }
void Logger::Info(const std::string& msg, const std::string& source)    { Log(LogLevel::Info, msg, source); }
void Logger::Warning(const std::string& msg, const std::string& source) { Log(LogLevel::Warning, msg, source); }
void Logger::Error(const std::string& msg, const std::string& source)   { Log(LogLevel::Error, msg, source); }
void Logger::Fatal(const std::string& msg, const std::string& source)   { Log(LogLevel::Fatal, msg, source); }

void Logger::Assert(bool condition, const std::string& message, const std::string& source) {
    if (!condition) {
        Log(LogLevel::Fatal, "ASSERTION FAILED: " + message, source);
#ifdef _WIN32
        __debugbreak();
#else
        __builtin_trap();
#endif
    }
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(mMutex);
    mEntries.clear();
}

void Logger::SetCallback(std::function<void(const LogEntry&)> callback) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCallback = callback;
}

void Logger::WriteToConsole(const LogEntry& entry) {
    std::ostringstream oss;
    oss << "[" << std::fixed << std::setprecision(2) << entry.time << "] "
        << "[" << LevelToString(entry.level) << "]";
    if (!entry.source.empty()) {
        oss << " [" << entry.source << "]";
    }
    oss << " " << entry.message;

    if (entry.level >= LogLevel::Error) {
        std::cerr << oss.str() << std::endl;
    } else {
        std::cout << oss.str() << std::endl;
    }
}

void Logger::WriteToFile(const LogEntry& entry) {
    if (mLogFile.empty()) return;

    std::ofstream file(mLogFile, std::ios::app);
    if (!file.is_open()) return;

    file << "[" << std::fixed << std::setprecision(2) << entry.time << "] "
         << "[" << LevelToString(entry.level) << "]";
    if (!entry.source.empty()) {
        file << " [" << entry.source << "]";
    }
    file << " " << entry.message << "\n";
}

} // namespace rpg
