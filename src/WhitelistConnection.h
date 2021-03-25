#include <sqlite3.h>
#include <string_view>
#include <nlohmann/json.hpp>

namespace Database
{
	class DbConnection
	{
	public:
		DbConnection(std::string_view pathname);
		~DbConnection();

		std::string_view GetErrorMessage() const;

		bool Execute(std::string_view sql);

	private:
		sqlite3* db;
		sqlite3_stmt* statement;
	};

	class WhitelistConnection
	{
	public:
		WhitelistConnection();
		~WhitelistConnection() = default;

		bool Includes(nlohmann::json logData);

	private:
		DbConnection dbConnection;
		const char* whitelistPath = "whitelist.sqlite";
	};
}