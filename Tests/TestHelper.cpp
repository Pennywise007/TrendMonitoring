#include "pch.h"

#include <DirsService.h>
#include <ITrendMonitoring.h>

#include <src/Utils.h>

#include "TestHelper.h"

////////////////////////////////////////////////////////////////////////////////
void TestHelper::resetMonitoringService()
{
    // сервис мониторинга
    ITrendMonitoring* monitoringService = get_monitoring_service();

    // сбрасываем все каналы
    for (size_t i = monitoringService->getNumberOfMonitoringChannels(); i > 0; --i)
    {
        monitoringService->removeMonitoringChannelByIndex(0);
    }
    monitoringService->setBotSettings(TelegramBotSettings());

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
