#include "WhitelistConnection.h"

namespace Database
{
	// DbConnection
	//
	DbConnection::DbConnection(std::string_view pathname)
	{
		sqlite3_open(pathname.data(), &db);
	}

	DbConnection::~DbConnection()
	{
		(void)sqlite3_close(db);
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
		: dbConnection(whitelistPath)
	{
	}

	bool WhitelistConnection::Includes(nlohmann::json logData)
	{
		// TODO: Implement here
		return false;
	}
}