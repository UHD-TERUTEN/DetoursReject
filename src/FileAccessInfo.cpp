#include "FileAccessInfo.h"

namespace LogData
{
    FileAccessInfo MakeFileAccessInfo(const std::string& functionName, BOOL returnValue)
    {
        return FileAccessInfo{ functionName, returnValue, GetLastError() };
    }
}