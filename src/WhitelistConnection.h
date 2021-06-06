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

		bool Insert(std::string_view sql);
		bool Includes(std::string_view sql);

	private:
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

		static WhitelistConnection& Instance();

		bool Includes(const nlohmann::json& logData);

	public:
		static const std::string path;

	private:
		DbConnection dbConnection;
	};

	class AgentDbConnection
	{
	public:
		AgentDbConnection();
		~AgentDbConnection() = default;

		static AgentDbConnection& Instance();

		bool Insert(const nlohmann::json& logData);

	public:
		static const std::string path;

	private:
		DbConnection dbConnection;
	};
}