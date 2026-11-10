#include <windows.h>
#include <string>
#include <vector>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int showCommand)
{
    std::vector<wchar_t> buffer(32768);
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size())
        return ERROR_BAD_PATHNAME;

    std::wstring directory(buffer.data(), length);
    directory.resize(directory.find_last_of(L"\\/") + 1);
    directory += L"runtime";
    const std::wstring executable = directory + L"\\Shadowless.exe";
    std::wstring command = L"\"" + executable + L"\"";
    if (arguments && *arguments) {
        command += L" ";
        command += arguments;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = static_cast<WORD>(showCommand);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        0, nullptr, directory.c_str(), &startup, &process)) {
        const DWORD error = GetLastError();
        const std::wstring message =
            L"Could not start Shadowless (Windows error " + std::to_wstring(error) +
            L"). Keep Shadowless.exe together with its runtime folder and extract the complete Portable archive.";
        MessageBoxW(nullptr, message.c_str(), L"Shadowless", MB_OK | MB_ICONERROR);
        return static_cast<int>(error);
    }

    CloseHandle(process.hThread);
    // Preserve completion and exit codes for diagnostics and shutdown commands.
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = EXIT_FAILURE;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
