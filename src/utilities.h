#pragma once
#include "FileInfo.h"
using namespace LogData;

#include "Logger.h"

#include <string>

#include <nlohmann/json.hpp>

bool IsExecutable(const FileInfo& fileInfo);

std::wstring ToWstring(const std::string& utf8);

std::string ToUtf8String(const wchar_t* unicode, const size_t unicode_size);

void InitLogger();

void Log(const nlohmann::json& json);

void LogException(const std::exception& e);

// concept Serializable = requires(T t, nlohmann::json j) { to_json(j, t); }
template <class FileInfoT>
nlohmann::json GetJson(const FileInfoT& info)
{
    nlohmann::json j{};
    to_json(j, info);
    return j;
}
