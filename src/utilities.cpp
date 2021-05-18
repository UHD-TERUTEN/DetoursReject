#include "utilities.h"

#include <cstdlib>
#include <mutex>
#include <algorithm>

static std::string GetFileExtension(const std::string& s)
{
    auto extpos = s.rfind('.');
    if (extpos != std::string::npos)
    {
        auto extension = s.substr(extpos);
        std::transform(std::begin(extension), std::end(extension), std::begin(extension), std::tolower);
        return extension;
    }
    return "";
}

wchar_t* GetCurrentProgramName()
{
    static wchar_t buffer[MAX_PATH]{};
    if (std::wcslen(buffer) == 0)
    {
        std::memset(buffer, 0, MAX_PATH);
        GetModuleFileNameW(NULL, buffer, MAX_PATH);
    }
    return buffer;
}

static std::string GetShortProgramName()
{
    auto wProgramName = GetCurrentProgramName();
    std::string programName = ToUtf8String(wProgramName, std::wcslen(wProgramName));
    auto pos = programName.rfind('\\');
    if (pos == std::string::npos)
        return "";
    return programName.substr(pos);
}

static std::string GetLogDirectoryName()
{
    if (const char* applicationRoot = std::getenv("FileAccessControlAgentRoot"))
        return std::string(applicationRoot) + R"(\RejectLogs)";
    return R"(C:\DetoursReject\Logs)";
}

bool IsExecutable(const FileInfo& fileInfo)
{
    return GetFileExtension(fileInfo.fileName) == ".exe"
        || GetFileExtension(fileInfo.fileName) == ".dll";
}

std::wstring ToWstring(const std::string& utf8)
{
    std::wstring wstr{};

    do
    {
        if (utf8.empty())
            break;
        //
        // getting required cch.
        //
        int required_cch = ::MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.c_str(), static_cast<int>(utf8.size()),
            nullptr, 0
        );
        if (0 == required_cch)
            break;
        //
        // allocate.
        //
        wstr.resize(required_cch);
        //
        // convert.
        //
        if (0 == ::MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            utf8.c_str(), static_cast<int>(utf8.size()),
            const_cast<wchar_t*>(wstr.c_str()), static_cast<int>(wstr.size())
        ))
        {
            break;
        }
    } while (false);
    return wstr;
}

// https://wendys.tistory.com/84
std::string ToUtf8String(const wchar_t* unicode, const size_t unicode_size)
{
    if ((nullptr == unicode) || (0 == unicode_size))
        return{};

    std::string utf8{};

    // getting required cch
    if (int required_cch = ::WideCharToMultiByte(   CP_UTF8,
                                                    WC_ERR_INVALID_CHARS,
                                                    unicode, static_cast<int>(unicode_size),
                                                    nullptr, 0,
                                                    nullptr, nullptr))
    {
        // allocate
        utf8.resize(required_cch);
    }
    else
        return {};

    // convert
    ::WideCharToMultiByte(  CP_UTF8,
                            WC_ERR_INVALID_CHARS,
                            unicode, static_cast<int>(unicode_size),
                            const_cast<char*>(utf8.c_str()), static_cast<int>(utf8.size()),
                            nullptr, nullptr);
    return utf8;
}

#include <filesystem>
#include <winternl.h>

using PWriteFile = BOOL(WINAPI*)(
    HANDLE       hFile,
    LPCVOID      lpBuffer,
    DWORD        nNumberOfBytesToWrite,
    LPDWORD      lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped
    );
using PNativeWriteFile = NTSTATUS(*)(
    HANDLE           FileHandle,
    HANDLE           Event,
    PIO_APC_ROUTINE  ApcRoutine,
    PVOID            ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID            Buffer,
    ULONG            Length,
    PLARGE_INTEGER   ByteOffset,
    PULONG           Key
    );
extern PWriteFile TrueWriteFile;
extern PNativeWriteFile TrueNtWriteFile;
extern bool hasNtdll;

std::mutex mutex{};

void InitLogger()
{
    auto logDirectoryName = GetLogDirectoryName();

    std::filesystem::create_directories(logDirectoryName);
    logger = CreateFileA((logDirectoryName + GetShortProgramName() + ".txt"s).c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
}

void Log(const nlohmann::json& json)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto text = json.dump() + "\n";

    if (hasNtdll)
    {
        IO_STATUS_BLOCK io;
        (void)TrueNtWriteFile(logger, NULL, NULL, NULL, &io, (PVOID)text.c_str(), (ULONG)text.length(), 0, NULL);
    }
    else
    {
        (void)TrueWriteFile(logger, (PVOID)text.c_str(), (ULONG)text.length(), 0, NULL);
    }
}

void LogException(const std::exception& e)
{
    Log({ { "error occurred", { "reason", e.what() } } });
}
