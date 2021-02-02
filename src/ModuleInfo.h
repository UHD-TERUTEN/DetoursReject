#pragma once
#include <string>
using namespace std::literals;

#include <Windows.h>

#include <nlohmann/json.hpp>

namespace LogData
{
    struct ModuleInfo
    {
        std::string companyName         = "(EMPTY)"s;
        std::string fileDescription     = "(EMPTY)"s;
        std::string fileVersion         = "(EMPTY)"s;
        std::string internalName        = "(EMPTY)"s;
        std::string legalCopyright      = "(EMPTY)"s;
        std::string originalFilename    = "(EMPTY)"s;
        std::string productName         = "(EMPTY)"s;
        std::string productVersion      = "(EMPTY)"s;
    };
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE
    (
        ModuleInfo,
        companyName,
        fileDescription,
        fileVersion,
        internalName,
        legalCopyright,
        originalFilename,
        productName,
        productVersion
    );
    ModuleInfo MakeModuleInfo(const std::string& fileName);
}
