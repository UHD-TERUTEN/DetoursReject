#pragma once
#include <cstdint>
#include <string>
using namespace std::literals;

#include <Windows.h>
#include <wchar.h>

#include <nlohmann/json.hpp>

namespace LogData
{
    struct FileInfo
    {
        std::string fileName        = "(EMPTY)"s;
        long long   fileSize        = 0LL;
        std::string creationTime    = "(EMPTY)"s;
        std::string lastWriteTime   = "(EMPTY)"s;
        bool        isHidden        = false;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE
    (
        FileInfo,
        fileName,
        fileSize,
        creationTime,
        lastWriteTime,
        isHidden
    );
    FileInfo MakeFileInfo(HANDLE hFile);
}