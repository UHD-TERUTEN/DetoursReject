#define WIN32_LEAN_AND_MEAN
#include "FileInfo.h"
#include "ModuleInfo.h"
#include "FileAccessInfo.h"
using namespace LogData;

#include <sstream>

#include "utilities.h"

#include <Windows.h>

#include <detours.h>

using PReadFile = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
static PReadFile TrueReadFile = ReadFile;

static constexpr char pipeName[] = R"(\\.\pipe\RejectLogPipe)";

static std::string cachedFileName = "";
static BOOL cachedResult = FALSE;


__declspec(dllexport)
BOOL WINAPI ReadFileOrReject(   HANDLE        hFile,
                                LPVOID        lpBuffer,
                                DWORD         nNumberOfBytesToRead,
                                LPDWORD       lpNumberOfBytesRead,
                                LPOVERLAPPED  lpOverlapped)
{
    nlohmann::json json{};
    BOOL ret{};
    try
    {
        json = {};
        {
            auto fileInfo = MakeFileInfo(hFile);
            json["fileInfo"] = GetJson(fileInfo);

            if (fileInfo.fileName == cachedFileName)
                return cachedResult ?
                TrueReadFile
                (
                    hFile,
                    lpBuffer,
                    nNumberOfBytesToRead,
                    lpNumberOfBytesRead,
                    lpOverlapped
                )
                : FALSE;

            cachedFileName = fileInfo.fileName;

            if (IsExecutable(fileInfo))
            {
                auto moduleInfo = MakeModuleInfo(fileInfo.fileName);
                json["moduleInfo"] = GetJson(moduleInfo);
            }
            else
                json["moduleInfo"] = nullptr;
        }
        {
            ret = TrueReadFile
            (
                hFile,
                lpBuffer,
                nNumberOfBytesToRead,
                lpNumberOfBytesRead,
                lpOverlapped
            );
            auto fileAccessInfo = MakeFileAccessInfo(__FUNCTION__, ret);
            json["fileAccessInfo"] = GetJson(fileAccessInfo);
        }
    }
    catch (std::exception& e)
    {
        json = { "error occurred", { "reason", e.what() } };
    }

    // Send the log to agent through a named pipe
    // And Receive a response which of reject or not
    // Both pop-up messages and DB connections are handled by the agent
    HANDLE pipe = CreateFileA(pipeName, GENERIC_ALL, 0, NULL, OPEN_EXISTING, 0, NULL);

    if ((pipe != INVALID_HANDLE_VALUE) && (GetLastError() != ERROR_PIPE_BUSY))
    {
        DWORD pipeMode = PIPE_READMODE_BYTE;
        SetNamedPipeHandleState(pipe, &pipeMode, NULL, NULL);

        char buffer[BUFSIZ]{};
        DWORD nbytes{};
        std::streamsize count{};
        std::stringstream ss{};

        ss << json;
        while (ss.read(buffer, BUFSIZ), (count = ss.gcount()) > 0)
            (void)WriteFile(pipe, buffer, count, &nbytes, NULL);

        (void)FlushFileBuffers(pipe);
        (void)memset(buffer, 0, BUFSIZ);

        if (ReadFile(pipe, buffer, 5, &nbytes, NULL))
        {
            ret = ("True"s == buffer);
            (void)FlushFileBuffers(pipe);
        }
    }
    CloseHandle(pipe);
    return (cachedResult = ret);
}

void WINAPI ProcessAttach(  HMODULE hModule,
                            DWORD   ul_reason_for_call,
                            LPVOID  lpReserved)
{
    DetourRestoreAfterWith();

    DisableThreadLibraryCalls(hModule);

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)TrueReadFile, ReadFileOrReject);
    DetourTransactionCommit();
}

void WINAPI ProcessDetach(  HMODULE hModule,
                            DWORD   ul_reason_for_call,
                            LPVOID  lpReserved)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)TrueReadFile, ReadFileOrReject);
    DetourTransactionCommit();
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
