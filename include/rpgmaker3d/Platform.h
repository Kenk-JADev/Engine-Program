#pragma once
#include <string>

namespace rpg {

class Platform {
public:
    static std::string GetExecutablePath();
    static std::string GetWorkingDirectory();
    static std::string GetAppDataPath(const std::string& appName);
    static std::string GetDocumentsPath();

    static bool CreateDirectory(const std::string& path);
    static bool FileExists(const std::string& path);
    static bool DirectoryExists(const std::string& path);

    static void OpenFolder(const std::string& path);
    static void OpenURL(const std::string& url);

    // Windows-spezifisch
    static void SetDPIAware();
    static std::string GetWindowsVersion();

    // MessageBox für Fatal Errors
    static void ShowMessageBox(const std::string& title, const std::string& message, bool isError = false);
};

} // namespace rpg
