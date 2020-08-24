// тестирование мониторнга данных, получение дautoанных и результатов

#include "pch.h"
#include "resource.h"

#include <math.h>

#include "SamplesHelper.h"
#include "TestTrendMonitoring.h"
#include "TestHelper.h"

// Список защитых в данных каналов мониторинга
const size_t kMonitoringChannelsCount = 3;

////////////////////////////////////////////////////////////////////////////////
// Тестирование задания и управления списком каналов
TEST_F(MonitoringTestClass, CheckTrendMonitoring)
{
    // проверяем что список каналов совпадает с зашитым
    std::set<CString> monitoringChannels = m_monitoingService->getNamesOfAllChannels();

    EXPECT_EQ(monitoringChannels.size(), kMonitoringChannelsCount)          << "Список каналов мониторинга отличается от эталонного";
    EXPECT_EQ(*monitoringChannels.begin(), L"Прогибометр №1")               << "Список каналов мониторинга отличается от эталонного";
    EXPECT_EQ(*std::next(monitoringChannels.begin()), L"Прогибометр №2")    << "Список каналов мониторинга отличается от эталонного";
    EXPECT_EQ(*std::next(monitoringChannels.begin(), 2), L"Прогибометр №3") << "Список каналов мониторинга отличается от эталонного";

    // Проверка добавления каналов
    testAddChannels();

    // Проверка настройки параметров каналов
    testSetParamsToChannels();

    // Проверка управления списком каналов
    testChannelListManagement();

    // проверяем результирующий файл конфигурации
    compareWithResourceFile(get_service<TestHelper>().getConfigFilePath().c_str(),
                            IDR_TESTAPPCONFIG, L"TestAppConfig");

    // Проверка удаления каналов из списка
    testDelChannels();
}

////////////////////////////////////////////////////////////////////////////////
void MonitoringTestClass::SetUp()
{
    // сбрасывааем все настройки у сервиса которые уже могли появиться при более раннем использовании другими тестами
    get_service<TestHelper>().resetMonitoringService();
}

//----------------------------------------------------------------------------//
void MonitoringTestClass::checkModelAndRealChannelsData(const std::string& testDescr)
{
    auto reportText = [&testDescr](const size_t index, const std::string& extraText)
    {
        CStringA resText;
        resText.Format("%s: Различаются данные по каналу №%u, различаются %s.", testDescr.c_str(), index, extraText.c_str());
        return resText;
    };

    // индекс проверяемого канала
    size_t index = 0;
    // проверяем что у локального списка каналов данные совпадают
    for (const auto& modelChannelData : m_channelsData)
    {
        const MonitoringChannelData& serviceChannelData = m_monitoingService->getMonitoringChannelData(index);

        EXPECT_EQ(serviceChannelData.bNotify, serviceChannelData.bNotify) << reportText(index, "флаг включенности оповещений");
        EXPECT_EQ(serviceChannelData.channelName, serviceChannelData.channelName) << reportText(index, "названия каналов");
        EXPECT_EQ(serviceChannelData.monitoringInterval, serviceChannelData.monitoringInterval) << reportText(index, "интервалы мониторинга");

        if (isfinite(serviceChannelData.allarmingValue) && isfinite(serviceChannelData.allarmingValue))
            // если они не наны - сравниваем числа
            EXPECT_FLOAT_EQ(serviceChannelData.allarmingValue, serviceChannelData.allarmingValue) << reportText(index, "оповещательное значение");
        else
            // если кто-то нан то второй тоже должен быть наном
            EXPECT_EQ(isfinite(serviceChannelData.allarmingValue), isfinite(serviceChannelData.allarmingValue)) << reportText(index, "оповещательное значение");

        ++index;
    }

    EXPECT_EQ(m_monitoingService->getNumberOfMonitoringChannels(), m_channelsData.size()) << testDescr + ": Различаются количество каналов.";
}

//----------------------------------------------------------------------------//
void MonitoringTestClass::testAddChannels()
{
    // добавляем в списорк все каналы которые можем
    for (size_t ind = 0; ind < kMonitoringChannelsCount; ++ind)
    {
        EXPECT_EQ(m_monitoingService->addMonitoingChannel(), ind) << "После добавления в списке каналов почему-то больше каналов чем должно";
        m_channelsData.push_back(MonitoringChannelData());
        m_channelsData.back().channelName = L"Прогибометр №1";
    }

    // проверяем что параметры совпали
    checkModelAndRealChannelsData("Добавление каналов");
}

//----------------------------------------------------------------------------//
void MonitoringTestClass::testSetParamsToChannels()
{
    auto applyModelSettingsToChannel = [&](const size_t channelIndex, const MonitoringChannelData& modelChannelData)
    {
        m_monitoingService->changeMonitoingChannelNotify  (channelIndex, modelChannelData.bNotify);
        m_monitoingService->changeMonitoingChannelName    (channelIndex, modelChannelData.channelName);
        m_monitoingService->changeMonitoingChannelInterval(channelIndex, modelChannelData.monitoringInterval);
        m_monitoingService->changeMonitoingChannelAllarmingValue(channelIndex, modelChannelData.allarmingValue);
    };

    size_t index = 1;
    // меняем настройки среднего канала
    auto modelChannelData = std::next(m_channelsData.begin(), index);
    modelChannelData->bNotify = false;
    modelChannelData->channelName = L"Прогибометр №2";
    modelChannelData->monitoringInterval = MonitoringInterval::eThreeMonths;
    modelChannelData->allarmingValue = -1;
    applyModelSettingsToChannel(index, *modelChannelData);

    index = kMonitoringChannelsCount - 1;
    // меняем настройки последнего канала
    modelChannelData = std::next(m_channelsData.begin(), index);
    modelChannelData->bNotify = true;
    modelChannelData->channelName = L"Прогибометр №3";
    modelChannelData->monitoringInterval = MonitoringInterval::eOneDay;
    modelChannelData->allarmingValue = 100;
    applyModelSettingsToChannel(index, *modelChannelData);

    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Установка настроек");
}

//----------------------------------------------------------------------------//
void MonitoringTestClass::testChannelListManagement()
{
    // 0 Прогибометр 1
    // 1 Прогибометр 2      ↑
    // 2 Прогибометр 3
    EXPECT_EQ(m_monitoingService->moveUpMonitoingChannelByIndex(1), 0) << "После перемещения конала вверх его индекс не валиден";
    m_channelsData.splice(m_channelsData.begin(), m_channelsData, ++m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Перемещение каналов в списке");

    // 0 Прогибометр 2      ↑
    // 1 Прогибометр 1
    // 2 Прогибометр 3
    EXPECT_EQ(m_monitoingService->moveUpMonitoingChannelByIndex(0), 2) << "После перемещения вверх первого канала он должен оказаться в конце";
    m_channelsData.splice(m_channelsData.end(), m_channelsData, m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Перемещение каналов в списке");

    // 0 Прогибометр 1
    // 1 Прогибометр 3      ↓
    // 2 Прогибометр 2
    EXPECT_EQ(m_monitoingService->moveDownMonitoingChannelByIndex(1), 2) << "После перемещения вниз среднего канала он должен оказаться последним";
    m_channelsData.splice(m_channelsData.end(), m_channelsData, ++m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Перемещение каналов в списке");

    // 0 Прогибометр 1
    // 1 Прогибометр 2
    // 2 Прогибометр 3      ↓
    EXPECT_EQ(m_monitoingService->moveDownMonitoingChannelByIndex(2), 0) << "После перемещения вниз последнего канала он должен оказаться первым";
    m_channelsData.splice(m_channelsData.begin(), m_channelsData, --m_channelsData.end());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Перемещение каналов в списке");
}

void MonitoringTestClass::testDelChannels()
{
    // Итоговый список каналов
    // 0 Прогибометр 3
    // 1 Прогибометр 1
    // 2 Прогибометр 2

    // удаление элементов
    EXPECT_EQ(m_monitoingService->removeMonitoringChannelByIndex(1), 1) << "После удаления канала выделенный индекс не корректнен";
    m_channelsData.erase(++m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Удаление каналов из списка");
    EXPECT_EQ(m_monitoingService->removeMonitoringChannelByIndex(0), 0) << "После удаления канала выделенный индекс не корректнен";
    m_channelsData.erase(m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Удаление каналов из списка");
}
