#include "rpgmaker3d/Platform.h"
#include "rpgmaker3d/Logger.h"
#include <filesystem>
#include <cstdlib>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// Windows makros kollidieren mit unseren Methodennamen (CreateDirectory -> CreateDirectoryW)
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef CreateDirectoryA
#undef CreateDirectoryA
#endif
#ifdef CreateDirectoryW
#undef CreateDirectoryW
#endif
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace rpg {

std::string Platform::GetExecutablePath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::string(buf);
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if (len != -1) {
        buf[len] = '\0';
        return std::string(buf);
    }
    return "./rpgmaker3d";
#endif
}

std::string Platform::GetWorkingDirectory() {
    return std::filesystem::current_path().string();
}

std::string Platform::GetAppDataPath(const std::string& appName) {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        std::string full = std::string(path) + "\\" + appName;
        std::filesystem::create_directories(full);
        return full;
    }
    return ".\\" + appName;
#else
    const char* home = getenv("HOME");
    if (!home) home = ".";
    std::string full = std::string(home) + "/." + appName;
    std::filesystem::create_directories(full);
    return full;
#endif
}

std::string Platform::GetDocumentsPath() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, path))) {
        return std::string(path);
    }
    return ".";
#else
    const char* home = getenv("HOME");
    if (!home) home = ".";
    return std::string(home) + "/Documents";
#endif
}

bool Platform::CreateDirectory(const std::string& path) {
    try {
        return std::filesystem::create_directories(path);
    } catch (...) {
        return false;
    }
}

bool Platform::FileExists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool Platform::DirectoryExists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

void Platform::OpenFolder(const std::string& path) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#else
    std::string cmd = "xdg-open \"" + path + "\" &";
    system(cmd.c_str());
#endif
}

void Platform::OpenURL(const std::string& url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
#else
    std::string cmd = "xdg-open \"" + url + "\" &";
    system(cmd.c_str());
#endif
}

void Platform::SetDPIAware() {
#ifdef _WIN32
    // Für Windows 10+ high DPI
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32) {
        typedef BOOL(WINAPI* SetProcessDPIAwareFunc)();
        auto setDPIAware = (SetProcessDPIAwareFunc)GetProcAddress(user32, "SetProcessDPIAware");
        if (setDPIAware) setDPIAware();
        FreeLibrary(user32);
    }
    // Try SetProcessDpiAwarenessContext (Windows 10 1703+)
    HMODULE shcore = LoadLibraryA("Shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI* SetProcessDpiAwarenessFunc)(int);
        auto setAwareness = (SetProcessDpiAwarenessFunc)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (setAwareness) setAwareness(2); // PROCESS_PER_MONITOR_DPI_AWARE
        FreeLibrary(shcore);
    }
#endif
}

std::string Platform::GetWindowsVersion() {
#ifdef _WIN32
    OSVERSIONINFOA vi;
    ZeroMemory(&vi, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
#pragma warning(push)
#pragma warning(disable:4996)
    GetVersionExA(&vi);
#pragma warning(pop)
    return std::to_string(vi.dwMajorVersion) + "." + std::to_string(vi.dwMinorVersion) + " Build " + std::to_string(vi.dwBuildNumber);
#else
    return "Non-Windows";
#endif
}

void Platform::ShowMessageBox(const std::string& title, const std::string& message, bool isError) {
#ifdef _WIN32
    UINT type = MB_OK;
    if (isError) type |= MB_ICONERROR;
    else type |= MB_ICONINFORMATION;
    MessageBoxA(nullptr, message.c_str(), title.c_str(), type);
#else
    RPG_LOG_ERROR(title + ": " + message);
#endif
}

} // namespace rpg
