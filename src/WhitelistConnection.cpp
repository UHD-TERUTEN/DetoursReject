#include "WhitelistConnection.h"
#include "utilities.h"

namespace Database
{
	static std::string GetWhitelistPathName()
	{
		if (const char* applicationRoot = std::getenv("FileAccessControlAgentRoot"))
			return applicationRoot;
		return R"(C:\Whitelists)";
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

	bool DbConnection::Execute(std::string_view sql)
	{
		if (sqlite3_prepare_v2(db, sql.data(), sql.size(), &statement, NULL) != SQLITE_OK)
			return false;

		switch (sqlite3_step(statement))
		{
		case SQLITE_DONE:
		case SQLITE_ROW:
			break;
		case SQLITE_BUSY:
		case SQLITE_ERROR:
		case SQLITE_MISUSE:
		default:
			return false;
		}

		return (sqlite3_finalize(statement) == SQLITE_OK);
	}

	// WhitelistConnection
	//
	WhitelistConnection::WhitelistConnection()
		: dbConnection(GetWhitelistPathName() + whitelistPath)
	{
	}

	bool WhitelistConnection::Includes(nlohmann::json logData)
	{
		// TODO: Implement here
		return false;
	}
}