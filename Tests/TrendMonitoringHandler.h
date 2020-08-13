// Сервис для работы с сервисом мониторинга, защищает реальный конфигурационный файл

#pragma once

#include <filesystem>

#include <ITrendMonitoring.h>
#include <Singleton.h>

////////////////////////////////////////////////////////////////////////////////
// Сервис работы с мониторингов данных, используется для того чтобы у сервиса был получатель сообщений
// и он не кидался ассертами что никто не обрабатывает результаты,
// а так же для сохранения реального конфигурационного файла
class TrendMonitoringHandler
{
    friend class CSingleton<TrendMonitoringHandler>;

public:
    TrendMonitoringHandler();
    ~TrendMonitoringHandler();

public:
    // Сброс настроек сервиса мониторинга
    void resetMonitoringService();
    // получение пути к текущему использующемуся файлу конфигурации
    std::filesystem::path getConfigFilePath();

private:
    // получение пути к сохаренной копии реального файла конфигурации
    std::filesystem::path getCopyConfigFilePath();
};
