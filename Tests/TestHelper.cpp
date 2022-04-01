#include "pch.h"

#include <DirsService.h>
#include <include/ITrendMonitoring.h>

#include "src/TrendMonitoring.h"
#include <src/Utils.h>

#include "TestHelper.h"


////////////////////////////////////////////////////////////////////////////////
void TestHelper::resetMonitoringService()
{
    // сервис мониторинга
    ITrendMonitoring* monitoringService = GetInterface<ITrendMonitoring>();

    // сбрасываем все каналы
    for (size_t i = monitoringService->GetNumberOfMonitoringChannels(); i > 0; --i)
    {
        monitoringService->RemoveMonitoringChannelByIndex(i - 1);
    }

    // reset mock
    TrendMonitoring* monitoring = dynamic_cast<TrendMonitoring*>(monitoringService);
    ASSERT_TRUE(!!monitoring);
    monitoring->installTelegramBot(nullptr);

    monitoringService->SetBotSettings(telegram::bot::TelegramBotSettings());

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
