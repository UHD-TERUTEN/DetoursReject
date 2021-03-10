#include "pch.h"
#include "../src/FileAccessInfo.h"
#include "../src/FileAccessInfo.cpp"
using namespace LogData;

#include <filesystem>
using namespace std::filesystem;

TEST(FileAccessInfoTest, MakeFileAccessInfoOfThisFile)
{
  auto file = CreateFileA(R"(..\..\test\FileInfo.cpp)",
                          GENERIC_READ,
                          FILE_SHARE_READ,
                          NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);

  ASSERT_NE(file, INVALID_HANDLE_VALUE);

  char buffer[4096]{};
  DWORD nbytes{};

  auto ret = ReadFile(file, buffer, sizeof buffer, &nbytes, NULL);

  auto fileInfo = MakeFileAccessInfo("ReadFile", ret);

  EXPECT_STREQ(fileInfo.functionName.c_str(), "ReadFile");
  EXPECT_TRUE(fileInfo.returnValue);
  EXPECT_EQ(fileInfo.errorCode, 0);
}
