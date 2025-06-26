#pragma once

#include <string>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <ShlObj.h>

namespace F::NamedPipe {

inline std::ofstream &LogFile() {
    static std::ofstream logFile("C:\\pipe_log.txt", std::ios::app);
    return logFile;
}

inline void Log(const std::string &message) {
    auto &file = LogFile();
    if (file.is_open()) {
        file << message << std::endl;
        file.flush();
    }
    OutputDebugStringA(("NamedPipe: " + message + "\n").c_str());
}

inline std::string GetErrorMessage(DWORD error) {
    char *buffer = nullptr;
    const size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, nullptr);
    std::string msg(buffer ? buffer : "", size);
    LocalFree(buffer);
    return msg;
}

// Returns "%APPDATA%\\Amalgam". Creates a fallback path via SHGetFolderPath when needed.
inline std::string GetAmalgamAppDataPath() {
    char *envPath = nullptr;
    size_t len = 0;
    if (_dupenv_s(&envPath, &len, "APPDATA") == 0 && envPath) {
        std::string amalgam = std::string(envPath) + "\\Amalgam";
        free(envPath);
        Log("AppData Amalgam path: " + amalgam);
        return amalgam;
    }

    char fallback[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, fallback))) {
        std::string amalgam = std::string(fallback) + "\\Amalgam";
        Log("AppData Amalgam path (fallback): " + amalgam);
        return amalgam;
    }

    Log("Failed to obtain AppData path");
    return "";
}

inline constexpr const char *PIPE_NAME = "\\\\.\\pipe\\AwootismBotPipe";

} // namespace F::NamedPipe 