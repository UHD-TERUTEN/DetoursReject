#pragma once
#include <string>

#include <Windows.h>

#include <nlohmann/json.hpp>

namespace LogData
{
	struct FileAccessInfo
	{
		std::string functionName;
		BOOL		returnValue;
		DWORD		errorCode;
	};
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE
	(
		FileAccessInfo,
		functionName,
		returnValue,
		errorCode
	);
	FileAccessInfo MakeFileAccessInfo(const std::string& functionName, int returnValue);
}