#include "pch.h"

#include <DirsService.h>

#include <src/Utils.h>

#include "TrendMonitoringHandler.h"

////////////////////////////////////////////////////////////////////////////////
TrendMonitoringHandler::TrendMonitoringHandler()
{
    // пока мы будем делать тесты могут попортиться реальные конфиги, если они есть сохраняем их и потом вернём
    const std::filesystem::path currentConfigPath(getConfigFilePath());
    if (std::filesystem::is_regular_file(currentConfigPath))
        std::filesystem::rename(currentConfigPath, getCopyConfigFilePath());
}

//----------------------------------------------------------------------------//
TrendMonitoringHandler::~TrendMonitoringHandler()
{
    // восстанавливаем сохраненный конфиг или удаляем созданный тестом
    const std::filesystem::path currentConfigPath(getConfigFilePath());
    const std::filesystem::path copyConfigPath(getCopyConfigFilePath());

    if (std::filesystem::is_regular_file(copyConfigPath))
        // если был реальный файл конфига сохранён копией - возвращаем его на место
        std::filesystem::rename(copyConfigPath, currentConfigPath);
    else
    {
        // подчищаем созданный файл с настройками
        EXPECT_TRUE(std::filesystem::is_regular_file(currentConfigPath)) << "Файл с настройками мониторинга не был создан!";
        EXPECT_TRUE(std::filesystem::remove(currentConfigPath)) << "Не удалось удалить файл";
    }
}

//----------------------------------------------------------------------------//
void TrendMonitoringHandler::resetMonitoringService()
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
std::filesystem::path TrendMonitoringHandler::getConfigFilePath()
{
    return (get_service<DirsService>().getExeDir() + kConfigFileName).GetString();
}

//----------------------------------------------------------------------------//
std::filesystem::path TrendMonitoringHandler::getCopyConfigFilePath()
{
    return (get_service<DirsService>().getExeDir() + kConfigFileName + L"_TestCopy").GetString();
}
