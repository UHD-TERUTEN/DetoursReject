#define WIN32_LEAN_AND_MEAN
#include "FileInfo.h"
#include "utilities.h"

#include <unordered_map>

#include <Windows.h>
#include <Shlwapi.h>    // SHFormatDateTimeA

static std::string RemoveLTRMark(std::string& utf8)
{
    size_t pos{};
    while ((pos = utf8.find("\xE2\x80\x8E")) != std::string::npos)
        utf8.erase(pos, 3);
    return utf8;
}

static uint64_t GetIntegerDateTime(FILETIME fileTime)
{
    return (uint64_t)(fileTime.dwHighDateTime) << 32
        | fileTime.dwLowDateTime;
}

namespace LogData
{
    FileInfo MakeFileInfo(HANDLE hFile)
    {
        BY_HANDLE_FILE_INFORMATION handleFileInfo{};
        FileInfo info{};

        if (GetFileInformationByHandle(hFile, &handleFileInfo))
        {
            constexpr auto PATHLEN = BUFSIZ * 4;
            wchar_t path[PATHLEN]{};
            
            if (auto size = GetFinalPathNameByHandle(hFile, path, PATHLEN, VOLUME_NAME_DOS);
                0 < size && size < PATHLEN)
            {
                info.fileName       = ToUtf8String(path + 4, size - 4); // remove prepended '\\?\'
                info.fileSize       = ((uint64_t)(handleFileInfo.nFileSizeHigh) << 32) | handleFileInfo.nFileSizeLow;
                info.creationTime   = GetIntegerDateTime(handleFileInfo.ftCreationTime);
                info.lastWriteTime  = GetIntegerDateTime(handleFileInfo.ftLastWriteTime);
                info.isHidden       = static_cast<bool>(handleFileInfo.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);
            }
        }
        return info;
    }

    FileInfo MakeFileInfo(std::string fileName)
    {
        FileInfo info{};
        info.fileName = fileName.substr(4); // remove prepended '\\?\'
        return info;
    }
}