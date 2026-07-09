#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <functional>

namespace rpg {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

struct LogEntry {
    LogLevel level;
    std::string message;
    std::string source;
    float time;
};

class Logger {
public:
    static Logger& Get();

    void SetLogFile(const std::string& path);
    void SetConsoleOutput(bool enabled);
    void SetMinLevel(LogLevel level);

    void Log(LogLevel level, const std::string& message, const std::string& source = "");
    void Trace(const std::string& msg, const std::string& source = "");
    void Debug(const std::string& msg, const std::string& source = "");
    void Info(const std::string& msg, const std::string& source = "");
    void Warning(const std::string& msg, const std::string& source = "");
    void Error(const std::string& msg, const std::string& source = "");
    void Fatal(const std::string& msg, const std::string& source = "");

    void Assert(bool condition, const std::string& message, const std::string& source = "");

    const std::vector<LogEntry>& GetEntries() const { return mEntries; }
    void Clear();

    void SetCallback(std::function<void(const LogEntry&)> callback);

    static const char* LevelToString(LogLevel level);

private:
    Logger() = default;
    void WriteToFile(const LogEntry& entry);
    void WriteToConsole(const LogEntry& entry);

    std::vector<LogEntry> mEntries;
    std::string mLogFile;
    bool mConsoleOutput = true;
    LogLevel mMinLevel = LogLevel::Debug;
    std::mutex mMutex;
    std::function<void(const LogEntry&)> mCallback;
};

// Kurzformen
#define RPG_LOG_TRACE(msg)   rpg::Logger::Get().Trace(msg, __FUNCTION__)
#define RPG_LOG_DEBUG(msg)   rpg::Logger::Get().Debug(msg, __FUNCTION__)
#define RPG_LOG_INFO(msg)    rpg::Logger::Get().Info(msg, __FUNCTION__)
#define RPG_LOG_WARN(msg)    rpg::Logger::Get().Warning(msg, __FUNCTION__)
#define RPG_LOG_ERROR(msg)   rpg::Logger::Get().Error(msg, __FUNCTION__)
#define RPG_LOG_FATAL(msg)   rpg::Logger::Get().Fatal(msg, __FUNCTION__)
#define RPG_ASSERT(cond, msg) rpg::Logger::Get().Assert(cond, msg, __FUNCTION__)

} // namespace rpg
