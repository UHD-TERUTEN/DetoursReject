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
        uint64_t    fileSize        = 0ULL;
        uint64_t    creationTime    = 0ULL;
        uint64_t    lastWriteTime   = 0ULL;
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
    FileInfo MakeFileInfo(std::string fileName);
}