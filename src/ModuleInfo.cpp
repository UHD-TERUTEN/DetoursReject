#define WIN32_LEAN_AND_MEAN
#include "ModuleInfo.h"
#include "utilities.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <memory>
#include <functional>

#include <WinBase.h>    // FindResource

namespace FileVersionGetter
{
    static std::wstring GetLanguageCodePageString(const wchar_t* buffer)
    {
        struct LanguageCodePage
        {
            WORD language, codePage;
        } *translate{};
        auto bufsize = sizeof(LanguageCodePage);
        std::wstringstream ss{};

        if (VerQueryValue(buffer, LR"(\VarFileInfo\Translation)", (LPVOID*)&translate, (PUINT)&bufsize))
        {
            ss.setf(std::ios::hex, std::ios::basefield);
            ss  << std::setw(4) << std::setfill(L'0') << translate[0].language
                << std::setw(4) << std::setfill(L'0') << translate[0].codePage;
        }
        return ss.str();
    }

    static std::wstring GetStringName(const wchar_t* buffer, const wchar_t* key)
    {
        auto langCodePage = GetLanguageCodePageString(buffer);
        if (langCodePage.empty())
            langCodePage = L"041204b0"s;

        std::wstringstream ss{};
        ss  << LR"(\StringFileInfo\)"
            << langCodePage
            << key;
        return ss.str();
    }

    inline static std::wstring GetCompanyName(const wchar_t* buffer)
    {
        return GetStringName(buffer, LR"(\CompanyName)");
    }

    inline static std::wstring GetFileDescription(const wchar_t* buffer)
    {
        return GetStringName(buffer, LR"(\FileDescription)");
    }

    inline static std::wstring GetFileVersion(const wchar_t* buffer)
    {
        return GetStringName(buffer, LR"(\FileVersion)");
    }

    inline static std::wstring GetInternalName(const wchar_t* buffer)
    {
        return GetStringName(buffer, LR"(\InternalName)");
    }

    inline static std::wstring GetLegalCopyright(const wchar_t* buffer)
    {
        return GetStringName(buffer, LR"(\LegalCopyright)");
    }

    inline static std::wstring GetOriginalFilename(const wchar_t* buffer)
    {
        return GetStringName(buffer, LR"(\OriginalFilename)");
    }

    inline static std::wstring GetProductName(const wchar_t* buffer)
    {
        return GetStringName(buffer, LR"(\ProductName)");
    }

    inline static std::wstring GetProductVersion(const wchar_t* buffer)
    {
        return GetStringName(buffer, LR"(\ProductVersion)");
    }
}

namespace LogData
{
    using namespace FileVersionGetter;

    // https://docs.microsoft.com/en-us/windows/win32/api/winver/nf-winver-verqueryvaluea
    // https://docs.microsoft.com/en-us/windows/win32/menurc/version-information
    ModuleInfo MakeModuleInfo(const std::string& fileName)
    {
        ModuleInfo info{};

        if (auto bufsize = GetFileVersionInfoSizeA(fileName.c_str(), NULL))
        {
            auto buffer = new wchar_t[bufsize] {};

            if (GetFileVersionInfoA(fileName.c_str(), 0, bufsize, buffer))
            {
                std::vector<std::pair<std::function<std::wstring(const wchar_t*)>, std::string&>> stringFileInfoGetters
                {
                    { GetCompanyName, info.companyName },
                    { GetFileDescription, info.fileDescription },
                    { GetFileVersion, info.fileVersion },
                    { GetInternalName, info.internalName },
                    { GetLegalCopyright, info.legalCopyright },
                    { GetOriginalFilename, info.originalFilename },
                    { GetProductName, info.productName },
                    { GetProductVersion, info.productVersion }
                };
                wchar_t* stringValue = NULL;   // static pointer, NEVER delete it

                for (auto getter : stringFileInfoGetters)
                {
                    auto GetStringFileInfo = getter.first;
                    auto& stringFileInfo = getter.second;

                    if (auto queryString = GetStringFileInfo(buffer);
                        queryString.length())
                    {
                        if (VerQueryValue(buffer, queryString.c_str(), (LPVOID*)&stringValue, (PUINT)&bufsize))
                            stringFileInfo = ToUtf8String(stringValue, bufsize - 1);
                    }
                }
            }
            delete[] buffer;
        }
        return info;
    }
}