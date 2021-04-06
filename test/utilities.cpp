#include "pch.h"
#include "../src/utilities.h"
#include "../src/utilities.cpp"
#include "../src/FileAccessInfo.h"
#include "../src/Logger.h"
#include <regex>
#include <filesystem>
#include <fstream>
using namespace std::filesystem;

std::ofstream logger{};


TEST(UtilitiesTest, CanExecute)
{
  FileInfo fileInfo{};
  fileInfo.fileName = R"(DetoursLog.dll)";

  EXPECT_TRUE(IsExecutable(fileInfo));
}

TEST(UtilitiesTest, ConvertUtf8AndWstring)
{
  char* bytes = "팔레트 (Feat. G-DRAGON)";

  auto wstr = ToWstring(bytes);
  auto utf8 = ToUtf8String(wstr.c_str(), wstr.size());

  EXPECT_STREQ(utf8.c_str(), bytes);
}

TEST(UtilitiesTest, GetJsonFileAccessInfo)
{
  FileAccessInfo fileAccessInfo = {"functionName", true, 1};
  nlohmann::json json({});
  json["fileAccessInfo"] = GetJson(fileAccessInfo);

  EXPECT_STREQ(json["fileAccessInfo"]["functionName"].get<std::string>().c_str(), "functionName");
  EXPECT_EQ(json["fileAccessInfo"]["returnValue"], 1);
  EXPECT_EQ(json["fileAccessInfo"]["errorCode"], 1);
}

TEST(UtilitiesTest, ExtractFileExtension)
{
  EXPECT_STREQ(GetFileExtension("utilities.cpp").c_str(),	".cpp");
  EXPECT_STREQ(GetFileExtension("test.exe.txt").c_str(),	".txt");
  EXPECT_STREQ(GetFileExtension(".gitignore").c_str(),		".gitignore");
}
