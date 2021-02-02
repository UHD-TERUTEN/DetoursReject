#define WIN32_LEAN_AND_MEAN
#include "FileInfo.h"
#include "utilities.h"

#include <unordered_map>

#include <Windows.h>
#include <Shlwapi.h>    // SHFormatDateTimeA

static std::string GetFormatDateTime(FILETIME fileTime)
{
    // convert FILETIME to format date time
    DWORD pdwFlags = FDTF_DEFAULT;
    wchar_t formatDateTime[BUFSIZ]{};
    std::string dateTime;

    auto size = SHFormatDateTime(&fileTime, &pdwFlags, formatDateTime, sizeof(formatDateTime));
    dateTime = ToUtf8String(formatDateTime, size);
    dateTime.erase(std::remove(std::begin(dateTime), std::end(dateTime), '?'), std::end(dateTime));
    return dateTime;
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
            TCHAR path[PATHLEN]{};

            if (auto size = GetFinalPathNameByHandle(hFile, path, PATHLEN, VOLUME_NAME_DOS);
                0 < size && size < PATHLEN)
            {
                info.fileName       = ToUtf8String(path + 4, size - 4); // remove prepended '\\?\'
                info.fileSize       = ((long long)(handleFileInfo.nFileSizeHigh) << 32) | handleFileInfo.nFileSizeLow;
                info.creationTime   = GetFormatDateTime(handleFileInfo.ftCreationTime);
                info.lastWriteTime  = GetFormatDateTime(handleFileInfo.ftLastWriteTime);
                info.isHidden       = static_cast<bool>(handleFileInfo.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN);
            }
        }
        return info;
    }
}