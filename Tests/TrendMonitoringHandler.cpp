#include "pch.h"

#include <DirsService.h>

#include <src/Utils.h>

#include "TrendMonitoringHandler.h"

////////////////////////////////////////////////////////////////////////////////
TrendMonitoringHandler::TrendMonitoringHandler()
{
    // ���� �� ����� ������ ����� ����� ����������� �������� �������, ���� ��� ���� ��������� �� � ����� �����
    const std::filesystem::path currentConfigPath(getConfigFilePath());
    if (std::filesystem::is_regular_file(currentConfigPath))
        std::filesystem::rename(currentConfigPath, getCopyConfigFilePath());
}

//----------------------------------------------------------------------------//
TrendMonitoringHandler::~TrendMonitoringHandler()
{
    // ��������������� ����������� ������ ��� ������� ��������� ������
    const std::filesystem::path currentConfigPath(getConfigFilePath());
    const std::filesystem::path copyConfigPath(getCopyConfigFilePath());

    if (std::filesystem::is_regular_file(copyConfigPath))
        // ���� ��� �������� ���� ������� ������� ������ - ���������� ��� �� �����
        std::filesystem::rename(copyConfigPath, currentConfigPath);
    else
    {
        // ��������� ��������� ���� � �����������
        EXPECT_TRUE(std::filesystem::is_regular_file(currentConfigPath)) << "���� � ����������� ����������� �� ��� ������!";
        EXPECT_TRUE(std::filesystem::remove(currentConfigPath)) << "�� ������� ������� ����";
    }
}

//----------------------------------------------------------------------------//
void TrendMonitoringHandler::resetMonitoringService()
{
    // ������ �����������
    ITrendMonitoring* monitoingService = get_monitoing_service();

    // ���������� ��� ������
    for (size_t i = monitoingService->getNumberOfMonitoringChannels(); i > 0; --i)
    {
        monitoingService->removeMonitoringChannelByIndex(0);
    }
    monitoingService->setBotSettings(TelegramBotSettings());

    // ������� ���������������� ����
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
