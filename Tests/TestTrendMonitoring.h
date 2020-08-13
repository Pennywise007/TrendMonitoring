#pragma once

#include <filesystem>

#include <gtest/gtest.h>

#include <ITrendMonitoring.h>

////////////////////////////////////////////////////////////////////////////////
// класс работы с трендами данных, используетс€ дл€ тестировани€ мониторинга
class MonitoringTestClass
    : public testing::Test
{
protected:
    void SetUp() override;

protected:  // “есты
    // ѕроверка добавлени€ каналов дл€ мониторинга
    void testAddChannels();
    // ѕроверка настройки параметров каналов
    void testSetParamsToChannels();
    // ѕроверка управлени€ списком каналов
    void testChannelListManagement();
    // ѕроверка удалени€ каналов из списка
    void testDelChannels();

private:    // ¬нутренние функции
    // ѕроверка что внутренний массив со списком каналов совпадает с заданным после выполнени€ теста
    void checkModelAndRealChannelsData(const std::string& testDescr);

protected:
    // список данных текущих каналов, частично должен совпадать с тем что в сервисе
    std::list<MonitoringChannelData> m_channelsData;
    // сервис мониторинга
    ITrendMonitoring* m_monitoingService = get_monitoing_service();
};