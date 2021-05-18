#include "FileAccessInfo.h"

namespace LogData
{
    FileAccessInfo MakeFileAccessInfo(const std::string& functionName, int returnValue)
    {
        return FileAccessInfo{ functionName, returnValue, GetLastError() };
    }
}