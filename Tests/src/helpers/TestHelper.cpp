#include "pch.h"

#include "mocks/DirServiceMock.h"

#include "TestHelper.h"

#include <ext/core/dependency_injection.h>
#include <ext/std/filesystem.h>

#include <include/ITrendMonitoring.h>

#include <src/Utils.h>

using namespace testing;

////////////////////////////////////////////////////////////////////////////////
void TestHelper::ResetAll() const
{
    ext::ServiceCollection& serviceCollection = ext::get_service<ext::ServiceCollection>();
    serviceCollection.ResetObjects();

    auto dirsServiceMock = serviceCollection.BuildServiceProvider()->GetInterface<DirServiceMock>();
    EXPECT_CALL(*dirsServiceMock.get(), GetZetSignalsDir()).WillRepeatedly(DoAll(
        Return((std::filesystem::get_exe_directory().append(LR"(Signals\)").c_str()))));
    // только потому что список каналов грузится из директории со сжатыми сигналами подменяем
    EXPECT_CALL(*dirsServiceMock.get(), GetZetCompressedDir()).WillRepeatedly(DoAll(
        Return((std::filesystem::get_exe_directory().append(LR"(Signals\)").c_str()))));

    // удаляем конфигурационный файл
    const std::filesystem::path currentConfigPath(getConfigFilePath());
    if (std::filesystem::is_regular_file(currentConfigPath))
        EXPECT_TRUE(std::filesystem::remove(currentConfigPath));
}

//----------------------------------------------------------------------------//
std::filesystem::path TestHelper::getConfigFilePath() const
{
    return std::filesystem::get_exe_directory().append(kConfigFileName);
}

//----------------------------------------------------------------------------//
std::filesystem::path TestHelper::getCopyConfigFilePath() const
{
    return std::filesystem::get_exe_directory().append(kConfigFileName + std::wstring(L"_TestCopy"));
}

//----------------------------------------------------------------------------//
std::filesystem::path TestHelper::getRestartFilePath() const
{
    return std::filesystem::get_exe_directory().append(kRestartSystemFileName);
}

//----------------------------------------------------------------------------//
std::filesystem::path TestHelper::getCopyRestartFilePath() const
{
    return std::filesystem::get_exe_directory().append(kRestartSystemFileName + std::wstring(L"_TestCopy"));
}
