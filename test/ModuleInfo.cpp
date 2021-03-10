#include "pch.h"
#include "../src/ModuleInfo.h"
#include "../src/ModuleInfo.cpp"
using namespace LogData;

TEST(ModuleInfoTest, MakeModuleInfoOfThisFile)
{
  auto moduleInfo = MakeModuleInfo("../../test/ModuleInfo.cpp");

  EXPECT_STREQ(moduleInfo.companyName.c_str(),      "(EMPTY)");
  EXPECT_STREQ(moduleInfo.fileDescription.c_str(),  "(EMPTY)");
  EXPECT_STREQ(moduleInfo.fileVersion.c_str(),      "(EMPTY)");
  EXPECT_STREQ(moduleInfo.internalName.c_str(),     "(EMPTY)");
  EXPECT_STREQ(moduleInfo.legalCopyright.c_str(),   "(EMPTY)");
  EXPECT_STREQ(moduleInfo.originalFilename.c_str(), "(EMPTY)");
  EXPECT_STREQ(moduleInfo.productName.c_str(),      "(EMPTY)");
  EXPECT_STREQ(moduleInfo.productVersion.c_str(),   "(EMPTY)");
}

TEST(ModuleInfoTest, MakeModuleInfoOfDll)
{
  auto moduleInfo = MakeModuleInfo(R"(DetoursLog.dll)");

  EXPECT_STREQ(moduleInfo.companyName.c_str(),      "UHD-TERUTEN");
  EXPECT_STREQ(moduleInfo.fileDescription.c_str(),  "DetoursLog");
  EXPECT_STREQ(moduleInfo.fileVersion.c_str(),      "1.3.0.0");
  EXPECT_STREQ(moduleInfo.internalName.c_str(),     "DetoursLog.dll");
  EXPECT_STREQ(moduleInfo.legalCopyright.c_str(),   "Copyright (C) 2021");
  EXPECT_STREQ(moduleInfo.originalFilename.c_str(), "DetoursLog.dll");
  EXPECT_STREQ(moduleInfo.productName.c_str(),      "DetoursLog");
  EXPECT_STREQ(moduleInfo.productVersion.c_str(),   "1.3.0.0");
}

TEST(ModuleInfoTest, GetKoreanCodePage)
{
  constexpr char* fileName = R"(DetoursLog.dll)";
  
  auto bufsize = GetFileVersionInfoSizeA(fileName, NULL);

  ASSERT_TRUE(bufsize);

  auto buffer = new wchar_t[bufsize] {};
  auto ret = GetFileVersionInfoA(fileName, 0, bufsize, buffer);

  ASSERT_TRUE(ret);

  auto langCodePage = GetLanguageCodePageString(buffer);

  EXPECT_STREQ(langCodePage.c_str(), L"041204b0");
}

TEST(ModuleInfoTest, GetFileVersionInfos)
{
  constexpr char* fileName = R"(DetoursLog.dll)";
  
  auto bufsize = GetFileVersionInfoSizeA(fileName, NULL);

  ASSERT_TRUE(bufsize);

  auto buffer = new wchar_t[bufsize] {};
  auto ret = GetFileVersionInfoA(fileName, 0, bufsize, buffer);

  ASSERT_TRUE(ret);

  EXPECT_STREQ(GetCompanyName(buffer).c_str(),      LR"(\StringFileInfo\041204b0\CompanyName)");
  EXPECT_STREQ(GetFileDescription(buffer).c_str(),  LR"(\StringFileInfo\041204b0\FileDescription)");
  EXPECT_STREQ(GetFileVersion(buffer).c_str(),      LR"(\StringFileInfo\041204b0\FileVersion)");
  EXPECT_STREQ(GetInternalName(buffer).c_str(),     LR"(\StringFileInfo\041204b0\InternalName)");
  EXPECT_STREQ(GetLegalCopyright(buffer).c_str(),   LR"(\StringFileInfo\041204b0\LegalCopyright)");
  EXPECT_STREQ(GetOriginalFilename(buffer).c_str(), LR"(\StringFileInfo\041204b0\OriginalFilename)");
  EXPECT_STREQ(GetProductName(buffer).c_str(),      LR"(\StringFileInfo\041204b0\ProductName)");
  EXPECT_STREQ(GetProductVersion(buffer).c_str(),   LR"(\StringFileInfo\041204b0\ProductVersion)");
}