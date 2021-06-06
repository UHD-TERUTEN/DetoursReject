#include "WhitelistConnection.h"
#include "utilities.h"

#include <sstream>

namespace Database
{
	static std::string GetWhitelistPathName()
	{
		if (const char* applicationRoot = std::getenv("FileAccessControlAgentRoot"))
			return applicationRoot + R"(\whitelist.sqlite)"s;
		return R"(C:\whitelist.sqlite)";
	}

	static std::string GetAgentDbPathName()
	{
		if (const char* applicationRoot = std::getenv("FileAccessControlAgentRoot"))
			return applicationRoot + R"(\publish\db.sqlite)"s;
		return R"(C:\db.sqlite)";
	}

	// DbConnection
	//
	DbConnection::DbConnection(std::string_view pathname)
		: db(nullptr)
		, statement(nullptr)
	{
		if (sqlite3_open(pathname.data(), &db) != SQLITE_OK)
			Log({ { "DbConnection", "sqlite3_open failed." } });
	}

	DbConnection::~DbConnection()
	{
		if (sqlite3_close(db) != SQLITE_OK)
			Log({ { "DbConnection", "sqlite3_close failed." } });
	}

	std::string_view DbConnection::GetErrorMessage() const
	{
		return sqlite3_errmsg(db);
	}

	bool DbConnection::Insert(std::string_view sql)
	{
		if (Execute(sql))
			return (sqlite3_finalize(statement) == SQLITE_OK);
		return false;
	}

	bool DbConnection::Includes(std::string_view sql)
	{
		if (Execute(sql))
		{
			int rows = sqlite3_data_count(statement);
			return (sqlite3_finalize(statement) == SQLITE_OK) && (rows > 0);
		}
		return false;
	}

	bool DbConnection::Execute(std::string_view sql)
	{
		if (auto e = sqlite3_prepare_v2(db, sql.data(), sql.size(), &statement, NULL) != SQLITE_OK)
			return false;

		switch (auto k = sqlite3_step(statement))
		{
		case SQLITE_DONE:
		case SQLITE_ROW:
			return true;
		case SQLITE_BUSY:
		case SQLITE_ERROR:
		case SQLITE_MISUSE:
		default:
			return false;
		}
	}

	// WhitelistConnection
	//
	WhitelistConnection::WhitelistConnection()
		: dbConnection(path)
	{
	}

	WhitelistConnection& WhitelistConnection::Instance()
	{
		static WhitelistConnection instance{};
		return instance;
	}

	static std::string GetTextValue(const nlohmann::json& value)
	{
		std::stringstream ss{};
		if (value.is_null())			ss << "NULL";
		else if (value.is_boolean())	ss << "'" << (value ? "True" : "False") << "'";
		else							ss << "'" << value.get<std::string>() << "'";
		return ss.str();
	}

	static std::vector<std::pair<std::string, std::string>> MakeQueryItems(const nlohmann::json& logData)
	{
		std::vector<std::pair<std::string, std::string>> res{};
		if (!logData["subject"].is_null())
		{
			if (!logData["subject"]["fileInfo"].is_null())
			{
				res.push_back(std::make_pair("subject_fileName", GetTextValue(logData["subject"]["fileInfo"]["fileName"])));
				res.push_back(std::make_pair("subject_fileSize", GetTextValue(logData["subject"]["fileInfo"]["fileSize"])));
				res.push_back(std::make_pair("subject_creationTime", GetTextValue(logData["subject"]["fileInfo"]["creationTime"])));
				res.push_back(std::make_pair("subject_lastWriteTime", GetTextValue(logData["subject"]["fileInfo"]["lastWriteTime"])));
				res.push_back(std::make_pair("subject_isHidden", GetTextValue(logData["subject"]["fileInfo"]["isHidden"])));
			}
			if (!logData["subject"]["moduleInfo"].is_null())
			{
				res.push_back(std::make_pair("subject_companyName", GetTextValue(logData["subject"]["moduleInfo"]["companyName"])));
				res.push_back(std::make_pair("subject_fileDescription", GetTextValue(logData["subject"]["moduleInfo"]["fileDescription"])));
				res.push_back(std::make_pair("subject_fileVersion", GetTextValue(logData["subject"]["moduleInfo"]["fileVersion"])));
				res.push_back(std::make_pair("subject_internalName", GetTextValue(logData["subject"]["moduleInfo"]["internalName"])));
				res.push_back(std::make_pair("subject_legalCopyright", GetTextValue(logData["subject"]["moduleInfo"]["legalCopyright"])));
				res.push_back(std::make_pair("subject_originalFilename", GetTextValue(logData["subject"]["moduleInfo"]["originalFilename"])));
				res.push_back(std::make_pair("subject_productName", GetTextValue(logData["subject"]["moduleInfo"]["productName"])));
				res.push_back(std::make_pair("subject_productVersion", GetTextValue(logData["subject"]["moduleInfo"]["productVersion"])));
			}
		}
		if (!logData["object"].is_null())
		{
			if (!logData["object"]["fileInfo"].is_null())
			{
				res.push_back(std::make_pair("object_fileName", GetTextValue(logData["object"]["fileInfo"]["fileName"])));
				res.push_back(std::make_pair("object_fileSize", GetTextValue(logData["object"]["fileInfo"]["fileSize"])));
				res.push_back(std::make_pair("object_creationTime", GetTextValue(logData["object"]["fileInfo"]["creationTime"])));
				res.push_back(std::make_pair("object_lastWriteTime", GetTextValue(logData["object"]["fileInfo"]["lastWriteTime"])));
				res.push_back(std::make_pair("object_isHidden", GetTextValue(logData["object"]["fileInfo"]["isHidden"])));
			}
			if (!logData["object"]["moduleInfo"].is_null())
			{
				res.push_back(std::make_pair("object_companyName", GetTextValue(logData["object"]["moduleInfo"]["companyName"])));
				res.push_back(std::make_pair("object_fileDescription", GetTextValue(logData["object"]["moduleInfo"]["fileDescription"])));
				res.push_back(std::make_pair("object_fileVersion", GetTextValue(logData["object"]["moduleInfo"]["fileVersion"])));
				res.push_back(std::make_pair("object_internalName", GetTextValue(logData["object"]["moduleInfo"]["internalName"])));
				res.push_back(std::make_pair("object_legalCopyright", GetTextValue(logData["object"]["moduleInfo"]["legalCopyright"])));
				res.push_back(std::make_pair("object_originalFilename", GetTextValue(logData["object"]["moduleInfo"]["originalFilename"])));
				res.push_back(std::make_pair("object_productName", GetTextValue(logData["object"]["moduleInfo"]["productName"])));
				res.push_back(std::make_pair("object_productVersion", GetTextValue(logData["object"]["moduleInfo"]["productVersion"])));
			}
		}
		if (!logData["operation"].is_null() && !logData["operation"]["fileAccessInfo"].is_null())
		{
			res.push_back(std::make_pair("operation_functionName", GetTextValue(logData["operation"]["fileAccessInfo"]["functionName"])));
			res.push_back(std::make_pair("operation_returnValue", GetTextValue(logData["operation"]["fileAccessInfo"]["returnValue"])));
			res.push_back(std::make_pair("operation_errorCode", GetTextValue(logData["operation"]["fileAccessInfo"]["errorCode"])));
		}
		if (!logData["environment"].is_null() && !logData["environment"]["timeInfo"].is_null())
		{
			res.push_back(std::make_pair("environment_isWorkingTime", GetTextValue(logData["environment"]["timeInfo"]["isWorkingTime"])));
		}
		return res;
	}

	bool WhitelistConnection::Includes(const nlohmann::json& logData)
	{
		try
		{
			// Ignore Logging
			if (logData.contains("subject")
				&& logData["subject"].contains("fileInfo")
				&& logData["subject"]["fileInfo"].contains("fileName"))
			{
				if (logData["object"]["fileInfo"]["fileName"] == WhitelistConnection::path
					|| logData["object"]["fileInfo"]["fileName"] == WhitelistConnection::path)
				{
					return true;
				}
			}
			std::stringstream selectQuery{};
			auto queryItems = MakeQueryItems(logData);

			selectQuery << R"(select * from Whitelist where )";
			for (auto item : queryItems)
			{
				selectQuery << item.first << R"( IS )" << item.second << R"( and )";
			}
			auto queryString = selectQuery.str();	// remove trailing " and "
			return dbConnection.Includes(queryString.substr(0, queryString.size() - 5));
		}
		catch (...)
		{
		}
		return false;
	}

	// AgentDbConnection
	//
	AgentDbConnection::AgentDbConnection()
		: dbConnection(path)
	{
	}

	AgentDbConnection& AgentDbConnection::Instance()
	{
		static AgentDbConnection instance{};
		return instance;
	}

	bool AgentDbConnection::Insert(const nlohmann::json& logData)
	{
		std::string programName = "NULL", fileName = "NULL", operation = "NULL";
		{
			if (logData.contains("subject")
				&& logData["subject"].contains("fileInfo")
				&& logData["subject"]["fileInfo"].contains("fileName"))
			{
				programName = logData["subject"]["fileInfo"]["fileName"];
			}
			if (logData.contains("object")
				&& logData["object"].contains("fileInfo")
				&& logData["object"]["fileInfo"].contains("fileName"))
			{
				fileName = logData["object"]["fileInfo"]["fileName"];
			}
			if (logData.contains("operation")
				&& logData["operation"].contains("fileAccessInfo")
				&& logData["operation"]["fileAccessInfo"].contains("functionName"))
			{
				operation = logData["operation"]["fileAccessInfo"]["functionName"];
			}
		}
		std::stringstream insertQuery{};
		insertQuery	<< "insert into LogList(ProgramName,FileName,Operation,PlainText,IsAccept) values('"s
					<< programName	<< "','"
					<< fileName		<< "','"
					<< operation	<< "','"
					<< logData		<< "',false)";
		auto queryString = insertQuery.str();
		return dbConnection.Insert(queryString);
	}

	const std::string WhitelistConnection::path = GetWhitelistPathName();
	const std::string AgentDbConnection::path = GetAgentDbPathName();
}