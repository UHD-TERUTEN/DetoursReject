#define WIN32_LEAN_AND_MEAN
#include "FileInfo.h"
#include "ModuleInfo.h"
#include "FileAccessInfo.h"
using namespace LogData;

#include "WhitelistConnection.h"
using namespace Database;

#include "Logger.h"
#include "utilities.h"

#include <sstream>

#include <Windows.h>

#include <detours.h>

#define LOG_PATH    R"(D:\logs\)"

using PReadFile = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
static PReadFile TrueReadFile = ReadFile;

static std::string cachedFileName = "";
static BOOL cachedResult = FALSE;

std::ofstream logger{};


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
        LogException(e);
    }

    WhitelistConnection whitelist{};

    if (!(ret = whitelist.Includes(json)))
    {
        Log(json);
    }
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
    InitLogger(LOG_PATH);

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
