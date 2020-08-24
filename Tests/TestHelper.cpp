#include "pch.h"

#include <DirsService.h>

#include <src/Utils.h>

#include "TestHelper.h"

////////////////////////////////////////////////////////////////////////////////
void TestHelper::resetMonitoringService()
{
    // сервис мониторинга
    ITrendMonitoring* monitoingService = get_monitoing_service();

    // сбрасываем все каналы
    for (size_t i = monitoingService->getNumberOfMonitoringChannels(); i > 0; --i)
    {
        monitoingService->removeMonitoringChannelByIndex(0);
    }
    monitoingService->setBotSettings(TelegramBotSettings());

    // удаляем конфигурационный файл
    const std::filesystem::path currentConfigPath(getConfigFilePath());
    if (std::filesystem::is_regular_file(currentConfigPath))
        EXPECT_TRUE(std::filesystem::remove(currentConfigPath));
}

//----------------------------------------------------------------------------//
std::filesystem::path TestHelper::getConfigFilePath() const
{
    return (get_service<DirsService>().getExeDir() + kConfigFileName).GetString();
}

//----------------------------------------------------------------------------//
std::filesystem::path TestHelper::getCopyConfigFilePath() const
{
    return (get_service<DirsService>().getExeDir() + kConfigFileName + L"_TestCopy").GetString();
}

//----------------------------------------------------------------------------//
std::filesystem::path TestHelper::getRestartFilePath() const
{
    return (get_service<DirsService>().getExeDir() + kRestartSystemFileName).GetString();
}

//----------------------------------------------------------------------------//
std::filesystem::path TestHelper::getCopyRestartFilePath() const
{
    return (get_service<DirsService>().getExeDir() + kRestartSystemFileName + L"_TestCopy").GetString();
}
