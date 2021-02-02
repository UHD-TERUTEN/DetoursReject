#pragma once
#include "FileInfo.h"
using namespace LogData;

#include <string>

#include <nlohmann/json.hpp>

bool IsExecutable(const FileInfo& fileInfo);

std::wstring ToWstring(const std::string& s);

std::string ToUtf8String(const wchar_t* unicode, const size_t unicode_size);

// concept Serializable = requires(T t, nlohmann::json j) { to_json(j, t); }
template <class FileInfoT>
nlohmann::json GetJson(const FileInfoT& info)
{
    nlohmann::json j{};
    to_json(j, info);
    return j;
}
