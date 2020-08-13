// ������ ��� ������ � �������� �����������, �������� �������� ���������������� ����

#pragma once

#include <filesystem>

#include <ITrendMonitoring.h>
#include <Singleton.h>

////////////////////////////////////////////////////////////////////////////////
// ������ ������ � ������������ ������, ������������ ��� ���� ����� � ������� ��� ���������� ���������
// � �� �� ������� ��������� ��� ����� �� ������������ ����������,
// � ��� �� ��� ���������� ��������� ����������������� �����
class TrendMonitoringHandler
{
    friend class CSingleton<TrendMonitoringHandler>;

public:
    TrendMonitoringHandler();
    ~TrendMonitoringHandler();

public:
    // ����� �������� ������� �����������
    void resetMonitoringService();
    // ��������� ���� � �������� ��������������� ����� ������������
    std::filesystem::path getConfigFilePath();

private:
    // ��������� ���� � ���������� ����� ��������� ����� ������������
    std::filesystem::path getCopyConfigFilePath();
};
