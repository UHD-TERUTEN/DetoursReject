#define WIN32_LEAN_AND_MEAN
#include "FileInfo.h"
#include "ModuleInfo.h"
#include "FileAccessInfo.h"
using namespace LogData;

#include "WhitelistConnection.h"
using namespace Database;

#include "utilities.h"

#include "Logger.h"

#include <string>
#include <chrono>
using namespace std::chrono;

#include <detours.h>
#include <winternl.h>

#define STATUS_FILE_INVALID 0xC0000098

using PCreateFileA = HANDLE (WINAPI*)(
    LPCSTR                lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile
);
using PCreateFileW = HANDLE (WINAPI*)(
    LPCWSTR               lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile
);
using PReadFile = BOOL (WINAPI*)(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
);
using PReadFileEx = BOOL (WINAPI*)(
    HANDLE                          hFile,
    LPVOID                          lpBuffer,
    DWORD                           nNumberOfBytesToRead,
    LPOVERLAPPED                    lpOverlapped,
    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);
using PWriteFile = BOOL (WINAPI*)(
    HANDLE       hFile,
    LPCVOID      lpBuffer,
    DWORD        nNumberOfBytesToWrite,
    LPDWORD      lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped
);
using PWriteFileEx = BOOL (WINAPI*)(
    HANDLE                          hFile,
    LPCVOID                         lpBuffer,
    DWORD                           nNumberOfBytesToWrite,
    LPOVERLAPPED                    lpOverlapped,
    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);
using PNativeCreateFile = NTSTATUS (*)(
    PHANDLE            FileHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK   IoStatusBlock,
    PLARGE_INTEGER     AllocationSize,
    ULONG              FileAttributes,
    ULONG              ShareAccess,
    ULONG              CreateDisposition,
    ULONG              CreateOptions,
    PVOID              EaBuffer,
    ULONG              EaLength
);
using PNativeOpenFile = NTSTATUS (*)(
    PHANDLE            FileHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK   IoStatusBlock,
    ULONG              ShareAccess,
    ULONG              OpenOptions
);
using PNativeReadFile = NTSTATUS (*)(
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
using PNativeWriteFile = NTSTATUS (*)(
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

// Windows APIs
//
       PCreateFileA TrueCreateFileA = CreateFileA;
static PCreateFileW TrueCreateFileW = CreateFileW;
static PReadFile    TrueReadFile    = ReadFile;
static PReadFileEx  TrueReadFileEx  = ReadFileEx;
       PWriteFile   TrueWriteFile   = WriteFile;
static PWriteFileEx TrueWriteFileEx = WriteFileEx;

// NT functions
//
static PNativeCreateFile TrueNtCreateFile   = (PNativeCreateFile)DetourFindFunction("ntdll.dll", "NtCreateFile");
static PNativeOpenFile TrueNtOpenFile       = (PNativeOpenFile)DetourFindFunction("ntdll.dll", "NtOpenFile");
static PNativeReadFile TrueNtReadFile       = (PNativeReadFile)DetourFindFunction("ntdll.dll", "NtReadFile");
       PNativeWriteFile TrueNtWriteFile     = (PNativeWriteFile)DetourFindFunction("ntdll.dll", "NtWriteFile");

// ZW functions
//
static PNativeCreateFile TrueZwCreateFile   = (PNativeCreateFile)DetourFindFunction("ntdll.dll", "ZwCreateFile");
static PNativeOpenFile TrueZwOpenFile       = (PNativeOpenFile)DetourFindFunction("ntdll.dll", "ZwOpenFile");
static PNativeReadFile TrueZwReadFile       = (PNativeReadFile)DetourFindFunction("ntdll.dll", "ZwReadFile");
static PNativeWriteFile TrueZwWriteFile     = (PNativeWriteFile)DetourFindFunction("ntdll.dll", "ZwWriteFile");

HANDLE logger{};

static std::string prevFunction{};
static BOOL prevRejected = FALSE;
static HANDLE prevFileHandle = NULL;


static nlohmann::json GetProcessInfo()
{
    static nlohmann::json json({});
    if (json.empty())
    {
        try
        {
            auto fileName = GetCurrentProgramName();

            auto hFile = TrueCreateFileW(fileName, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                auto fileInfo = MakeFileInfo(hFile);
                json["fileInfo"] = GetJson(fileInfo);

                if (IsExecutable(fileInfo))
                {
                    auto moduleInfo = MakeModuleInfo(fileInfo.fileName);
                    json["moduleInfo"] = GetJson(moduleInfo);
                }
                CloseHandle(hFile);
            }
        }
        catch (...)
        {
            throw;
        }
    }
    return json;
}

static bool IsWorkingTime(tm* currentTime)
{
    return (1 <= currentTime->tm_hour) && (currentTime->tm_hour < 9);
}

static nlohmann::json MakeEnvironmentInfo()
{
    nlohmann::json json({});
    try
    {
        auto t = system_clock::to_time_t(system_clock::now());
        tm currentTime{};
        if (gmtime_s(&currentTime, &t) == 0)
        {
            json["timeInfo"] =
            {
                { "isWorkingTime", IsWorkingTime(&currentTime) }
            };
        }
    }
    catch (...)
    {
        throw;
    }
    return json;
}

static nlohmann::json MakeLog(HANDLE hFile, const char* functionName)
{
    try
    {
        nlohmann::json subject{}, object({}), operation({}), environment{};

        subject = GetProcessInfo();
        {
            auto fileInfo = MakeFileInfo(hFile);
            object["fileInfo"] = GetJson(fileInfo);

            if (IsExecutable(fileInfo))
            {
                auto moduleInfo = MakeModuleInfo(fileInfo.fileName);
                object["moduleInfo"] = GetJson(moduleInfo);
            }
        }
        {
            auto fileAccessInfo = MakeFileAccessInfo(functionName, FALSE);
            operation["fileAccessInfo"] = GetJson(fileAccessInfo);
        }
        environment = MakeEnvironmentInfo();

        return nlohmann::json
        ({
            {"subject",     subject},
            {"object",      object},
            {"operation",   operation},
            {"environment", environment}
        });
    }
    catch (std::exception& e)
    {
        LogException(e);
        return {};
    }
}

static nlohmann::json MakeLog(std::string fileName, const char* functionName)
{
    try
    {
        nlohmann::json subject{}, object({}), operation({}), environment{};

        subject = GetProcessInfo();
        {
            auto fileInfo = MakeFileInfo(fileName);
            object["fileInfo"] = GetJson(fileInfo);
        }
        {
            auto fileAccessInfo = MakeFileAccessInfo(functionName, FALSE);
            operation["fileAccessInfo"] = GetJson(fileAccessInfo);
        }
        environment = MakeEnvironmentInfo();

        return nlohmann::json
        ({
            {"subject",     subject},
            {"object",      object},
            {"operation",   operation},
            {"environment", environment}
        });
    }
    catch (std::exception& e)
    {
        LogException(e);
        return {};
    }
}

// Windows APIs
//
__declspec(dllexport)
HANDLE WINAPI DetouredCreateFileA(
    LPCSTR                lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile
)
{
    HANDLE ret = NULL;
    try
    {
        auto log = MakeLog(lpFileName, __FUNCTION__);
        prevFunction = __FUNCTION__;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueCreateFileA
            (
                lpFileName,
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
HANDLE WINAPI DetouredCreateFileW(
    LPCWSTR               lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile
)
{
    HANDLE ret = NULL;
    try
    {
        auto log = MakeLog(ToUtf8String(lpFileName, wcslen(lpFileName)), __FUNCTION__);
        prevFunction = __FUNCTION__;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueCreateFileW
            (
                lpFileName,
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
BOOL WINAPI DetouredReadFile(
    HANDLE        hFile,
    LPVOID        lpBuffer,
    DWORD         nNumberOfBytesToRead,
    LPDWORD       lpNumberOfBytesRead,
    LPOVERLAPPED  lpOverlapped)
{
    BOOL ret = FALSE;
    try
    {
        auto log = MakeLog(hFile, __FUNCTION__);

        if (prevFunction == __FUNCTION__ && prevFileHandle == hFile)
        {
            if (!prevRejected)
                TrueReadFile
                (
                    hFile,
                    lpBuffer,
                    nNumberOfBytesToRead,
                    lpNumberOfBytesRead,
                    lpOverlapped
                );
            return prevRejected;
        }
        prevFunction = __FUNCTION__;
        prevFileHandle = hFile;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueReadFile
            (
                hFile,
                lpBuffer,
                nNumberOfBytesToRead,
                lpNumberOfBytesRead,
                lpOverlapped
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
BOOL WINAPI DetouredReadFileEx(
    HANDLE                          hFile,
    LPVOID                          lpBuffer,
    DWORD                           nNumberOfBytesToRead,
    LPOVERLAPPED                    lpOverlapped,
    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
)
{
    BOOL ret = FALSE;
    try
    {
        auto log = MakeLog(hFile, __FUNCTION__);

        if (prevFunction == __FUNCTION__ && prevFileHandle == hFile)
        {
            if (!prevRejected)
                TrueReadFileEx
                (
                    hFile,
                    lpBuffer,
                    nNumberOfBytesToRead,
                    lpOverlapped,
                    lpCompletionRoutine
                );
            return prevRejected;
        }
        prevFunction = __FUNCTION__;
        prevFileHandle = hFile;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueReadFileEx
            (
                hFile,
                lpBuffer,
                nNumberOfBytesToRead,
                lpOverlapped,
                lpCompletionRoutine
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
BOOL WINAPI DetouredWriteFile(
    HANDLE       hFile,
    LPCVOID      lpBuffer,
    DWORD        nNumberOfBytesToWrite,
    LPDWORD      lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped
)
{
    BOOL ret = FALSE;
    try
    {
        auto log = MakeLog(hFile, __FUNCTION__);

        if (prevFunction == __FUNCTION__ && prevFileHandle == hFile)
        {
            if (!prevRejected)
                TrueWriteFile
                (
                    hFile,
                    lpBuffer,
                    nNumberOfBytesToWrite,
                    lpNumberOfBytesWritten,
                    lpOverlapped
                );
            return prevRejected;
        }
        prevFunction = __FUNCTION__;
        prevFileHandle = hFile;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueWriteFile
            (
                hFile,
                lpBuffer,
                nNumberOfBytesToWrite,
                lpNumberOfBytesWritten,
                lpOverlapped
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
BOOL WINAPI DetouredWriteFileEx(
    HANDLE                          hFile,
    LPCVOID                         lpBuffer,
    DWORD                           nNumberOfBytesToWrite,
    LPOVERLAPPED                    lpOverlapped,
    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
)
{
    BOOL ret = FALSE;
    try
    {
        auto log = MakeLog(hFile, __FUNCTION__);

        if (prevFunction == __FUNCTION__ && prevFileHandle == hFile)
        {
            if (!prevRejected)
                TrueWriteFileEx
                (
                    hFile,
                    lpBuffer,
                    nNumberOfBytesToWrite,
                    lpOverlapped,
                    lpCompletionRoutine
                );
            return prevRejected;
        }
        prevFunction = __FUNCTION__;
        prevFileHandle = hFile;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueWriteFileEx
            (
                hFile,
                lpBuffer,
                nNumberOfBytesToWrite,
                lpOverlapped,
                lpCompletionRoutine
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

// NT functions
//
__declspec(dllexport)
NTSTATUS DetouredNtCreateFile
(
    PHANDLE            FileHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK   IoStatusBlock,
    PLARGE_INTEGER     AllocationSize,
    ULONG              FileAttributes,
    ULONG              ShareAccess,
    ULONG              CreateDisposition,
    ULONG              CreateOptions,
    PVOID              EaBuffer,
    ULONG              EaLength
)
{
    NTSTATUS ret = STATUS_FILE_INVALID;
    try
    {
        auto log = MakeLog(FileHandle, __FUNCTION__);
        prevFunction = __FUNCTION__;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueNtCreateFile
            (
                FileHandle,
                DesiredAccess,
                ObjectAttributes,
                IoStatusBlock,
                AllocationSize,
                FileAttributes,
                ShareAccess,
                CreateDisposition,
                CreateOptions,
                EaBuffer,
                EaLength
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
NTSTATUS DetouredNtOpenFile
(
    PHANDLE            FileHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK   IoStatusBlock,
    ULONG              ShareAccess,
    ULONG              OpenOptions
)
{
    NTSTATUS ret = STATUS_FILE_INVALID;
    try
    {
        auto log = MakeLog(FileHandle, __FUNCTION__);
        prevFunction = __FUNCTION__;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueNtOpenFile
            (
                FileHandle,
                DesiredAccess,
                ObjectAttributes,
                IoStatusBlock,
                ShareAccess,
                OpenOptions
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
NTSTATUS DetouredNtReadFile
(
    HANDLE           FileHandle,
    HANDLE           Event,
    PIO_APC_ROUTINE  ApcRoutine,
    PVOID            ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID            Buffer,
    ULONG            Length,
    PLARGE_INTEGER   ByteOffset,
    PULONG           Key
)
{

    NTSTATUS ret = STATUS_FILE_INVALID;
    try
    {
        auto log = MakeLog(FileHandle, __FUNCTION__);
        prevFunction = __FUNCTION__;
        prevFileHandle = FileHandle;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueNtReadFile
            (
                FileHandle,
                Event,
                ApcRoutine,
                ApcContext,
                IoStatusBlock,
                Buffer,
                Length,
                ByteOffset,
                Key
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
NTSTATUS DetouredNtWriteFile
(
    HANDLE           FileHandle,
    HANDLE           Event,
    PIO_APC_ROUTINE  ApcRoutine,
    PVOID            ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID            Buffer,
    ULONG            Length,
    PLARGE_INTEGER   ByteOffset,
    PULONG           Key
)
{
    NTSTATUS ret = STATUS_FILE_INVALID;
    try
    {
        auto log = MakeLog(FileHandle, __FUNCTION__);
        prevFunction = __FUNCTION__;
        prevFileHandle = FileHandle;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueNtWriteFile
            (
                FileHandle,
                Event,
                ApcRoutine,
                ApcContext,
                IoStatusBlock,
                Buffer,
                Length,
                ByteOffset,
                Key
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}


// ZW functions
//
__declspec(dllexport)
NTSTATUS DetouredZwCreateFile
(
    PHANDLE            FileHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK   IoStatusBlock,
    PLARGE_INTEGER     AllocationSize,
    ULONG              FileAttributes,
    ULONG              ShareAccess,
    ULONG              CreateDisposition,
    ULONG              CreateOptions,
    PVOID              EaBuffer,
    ULONG              EaLength
)
{
    NTSTATUS ret = STATUS_FILE_INVALID;
    try
    {
        auto log = MakeLog(FileHandle, __FUNCTION__);
        prevFunction = __FUNCTION__;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueZwCreateFile
            (
                FileHandle,
                DesiredAccess,
                ObjectAttributes,
                IoStatusBlock,
                AllocationSize,
                FileAttributes,
                ShareAccess,
                CreateDisposition,
                CreateOptions,
                EaBuffer,
                EaLength
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
NTSTATUS DetouredZwOpenFile
(
    PHANDLE            FileHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK   IoStatusBlock,
    ULONG              ShareAccess,
    ULONG              OpenOptions
)
{
    NTSTATUS ret = STATUS_FILE_INVALID;
    try
    {
        auto log = MakeLog(FileHandle, __FUNCTION__);
        prevFunction = __FUNCTION__;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueZwOpenFile
            (
                FileHandle,
                DesiredAccess,
                ObjectAttributes,
                IoStatusBlock,
                ShareAccess,
                OpenOptions
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
NTSTATUS DetouredZwReadFile
(
    HANDLE           FileHandle,
    HANDLE           Event,
    PIO_APC_ROUTINE  ApcRoutine,
    PVOID            ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID            Buffer,
    ULONG            Length,
    PLARGE_INTEGER   ByteOffset,
    PULONG           Key
)
{
    NTSTATUS ret = STATUS_FILE_INVALID;
    try
    {
        auto log = MakeLog(FileHandle, __FUNCTION__);
        prevFunction = __FUNCTION__;
        prevFileHandle = FileHandle;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueZwReadFile
            (
                FileHandle,
                Event,
                ApcRoutine,
                ApcContext,
                IoStatusBlock,
                Buffer,
                Length,
                ByteOffset,
                Key
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

__declspec(dllexport)
NTSTATUS DetouredZwWriteFile
(
    HANDLE           FileHandle,
    HANDLE           Event,
    PIO_APC_ROUTINE  ApcRoutine,
    PVOID            ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID            Buffer,
    ULONG            Length,
    PLARGE_INTEGER   ByteOffset,
    PULONG           Key
)
{
    NTSTATUS ret = STATUS_FILE_INVALID;
    try
    {
        auto log = MakeLog(FileHandle, __FUNCTION__);
        prevFunction = __FUNCTION__;
        prevFileHandle = FileHandle;

        if (prevRejected = !WhitelistConnection::Instance().Includes(log))
        {
            NotifyReject(log);
            Log(log);
        }
        else
        {
            ret = TrueZwWriteFile
            (
                FileHandle,
                Event,
                ApcRoutine,
                ApcContext,
                IoStatusBlock,
                Buffer,
                Length,
                ByteOffset,
                Key
            );
        }
    }
    catch (std::exception& e)
    {
        LogException(e);
    }
    return ret;
}

static bool HasNtdll()
{
    static const char* ntdll = "ntdll.dll";

    for (HMODULE hModule = NULL; (hModule = DetourEnumerateModules(hModule)) != NULL;)
    {
        char moduleName[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hModule, moduleName, sizeof(moduleName) - 1)
            && !std::strcmp(moduleName, ntdll))
        {
            return true;
        }
    }
    return false;
}

bool hasNtdll = HasNtdll();

void WINAPI ProcessAttach(  HMODULE hModule,
                            DWORD   ul_reason_for_call,
                            LPVOID  lpReserved)
{
    InitLogger();
    try
    {
        WhitelistConnection::Instance();
        AgentDbConnection::Instance();
        GetProcessInfo();   // Caching result
    }
    catch (std::exception& e)
    {
        LogException(e);
    }

    DetourRestoreAfterWith();

    DisableThreadLibraryCalls(hModule);

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (hasNtdll)
    {
        DetourAttach(&(PVOID&)TrueNtCreateFile, DetouredNtCreateFile);
        DetourAttach(&(PVOID&)TrueNtOpenFile, DetouredNtOpenFile);
        DetourAttach(&(PVOID&)TrueNtReadFile, DetouredNtReadFile);
        DetourAttach(&(PVOID&)TrueNtWriteFile, DetouredNtWriteFile);

        DetourAttach(&(PVOID&)TrueZwCreateFile, DetouredZwCreateFile);
        DetourAttach(&(PVOID&)TrueZwOpenFile, DetouredZwOpenFile);
        DetourAttach(&(PVOID&)TrueZwReadFile, DetouredZwReadFile);
        DetourAttach(&(PVOID&)TrueZwWriteFile, DetouredZwWriteFile);
    }
    else
    {
        DetourAttach(&(PVOID&)TrueCreateFileA, DetouredCreateFileA);
        DetourAttach(&(PVOID&)TrueCreateFileW, DetouredCreateFileW);
        DetourAttach(&(PVOID&)TrueReadFile, DetouredReadFile);
        DetourAttach(&(PVOID&)TrueReadFileEx, DetouredReadFileEx);
        DetourAttach(&(PVOID&)TrueWriteFile, DetouredWriteFile);
        DetourAttach(&(PVOID&)TrueWriteFileEx, DetouredWriteFileEx);
    }
    DetourTransactionCommit();
}

void WINAPI ProcessDetach(  HMODULE hModule,
                            DWORD   ul_reason_for_call,
                            LPVOID  lpReserved)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (hasNtdll)
    {
        DetourDetach(&(PVOID&)TrueNtCreateFile, DetouredNtCreateFile);
        DetourDetach(&(PVOID&)TrueNtOpenFile, DetouredNtOpenFile);
        DetourDetach(&(PVOID&)TrueNtReadFile, DetouredNtReadFile);
        DetourDetach(&(PVOID&)TrueNtWriteFile, DetouredNtWriteFile);

        DetourDetach(&(PVOID&)TrueZwCreateFile, DetouredZwCreateFile);
        DetourDetach(&(PVOID&)TrueZwOpenFile, DetouredZwOpenFile);
        DetourDetach(&(PVOID&)TrueZwReadFile, DetouredZwReadFile);
        DetourDetach(&(PVOID&)TrueZwWriteFile, DetouredZwWriteFile);
    }
    else
    {
        DetourDetach(&(PVOID&)TrueCreateFileA, DetouredCreateFileA);
        DetourDetach(&(PVOID&)TrueCreateFileW, DetouredCreateFileW);
        DetourDetach(&(PVOID&)TrueReadFile, DetouredReadFile);
        DetourDetach(&(PVOID&)TrueReadFileEx, DetouredReadFileEx);
        DetourDetach(&(PVOID&)TrueWriteFile, DetouredWriteFile);
        DetourDetach(&(PVOID&)TrueWriteFileEx, DetouredWriteFileEx);
    }
    DetourTransactionCommit();

    CloseLogger();
}


BOOL APIENTRY DllMain( HMODULE  hModule,
                       DWORD    ul_reason_for_call,
                       LPVOID   lpReserved)
{
    if (DetourIsHelperProcess())
        return TRUE;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ProcessAttach(hModule, ul_reason_for_call, lpReserved);
        break;
    case DLL_PROCESS_DETACH:
        ProcessDetach(hModule, ul_reason_for_call, lpReserved);
        break;
    }
    return TRUE;
}
