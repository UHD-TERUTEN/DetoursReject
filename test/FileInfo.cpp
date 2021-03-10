#include "pch.h"
#include "../src/FileInfo.h"
#include "../src/FIleInfo.cpp"
using namespace LogData;

#include "../src/utilities.h"

#include <filesystem>
using namespace std::filesystem;

TEST(FileInfoTest, MakeFileInfoOfThisFile)
{
    auto file = CreateFileA(R"(..\..\test\FileInfo.cpp)",
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    ASSERT_NE(file, INVALID_HANDLE_VALUE);

    auto fileInfo = MakeFileInfo(file);
    std::string creationTime = R"(2021‎-0‎3-10 ‏‎오후 9:47)";

    EXPECT_STREQ(fileInfo.fileName.c_str(), absolute(R"(..\..\test\FileInfo.cpp)").string().c_str());
    EXPECT_EQ(fileInfo.fileSize, 1504);
    EXPECT_STREQ(fileInfo.creationTime.c_str(), RemoveLTRMark(creationTime).c_str());
    EXPECT_FALSE(fileInfo.isHidden);
}

TEST(FileInfoTest, GetFormatDateTimeOfThisFile)
{
    auto file = CreateFileA(R"(..\..\test\FileInfo.cpp)",
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    ASSERT_NE(file, INVALID_HANDLE_VALUE);

    FILETIME creationTime{};

    auto ret = GetFileTime(file, &creationTime, NULL, NULL);

    ASSERT_TRUE(ret);

    auto formatDateTime = GetFormatDateTime(creationTime);
    std::string comparer = R"(2021‎-0‎3-10 ‏‎오후 9:47)";

    EXPECT_STREQ(formatDateTime.c_str(), RemoveLTRMark(comparer).c_str());
}