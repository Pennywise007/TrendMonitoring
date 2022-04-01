// тестирование мониторнга данных, получение дautoанных и результатов

#include "pch.h"
#include "resource.h"

#include <math.h>

#include "SamplesHelper.h"
#include "TestTrendMonitoring.h"
#include "TestHelper.h"

#include "gmock/gmock-actions.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock-spec-builders.h"

using namespace ::testing;

// Список защитых в данных каналов мониторинга
const size_t kMonitoringChannelsCount = 3;

////////////////////////////////////////////////////////////////////////////////
// Тестирование задания и управления списком каналов
TEST_F(MonitoringTestClass, CheckTrendMonitoring)
{
    // проверяем что список каналов совпадает с зашитым
    const auto monitoringChannels = m_monitoringService->GetNamesOfAllChannels();

    EXPECT_EQ(monitoringChannels.size(), kMonitoringChannelsCount)          << "Список каналов мониторинга отличается от эталонного";

    auto monitoringChannel = monitoringChannels.begin();
    EXPECT_EQ(*monitoringChannel, L"Прогибометр №1") << "Список каналов мониторинга отличается от эталонного";
    EXPECT_EQ(*++monitoringChannel, L"Прогибометр №2") << "Список каналов мониторинга отличается от эталонного";
    EXPECT_EQ(*++monitoringChannel, L"Прогибометр №3") << "Список каналов мониторинга отличается от эталонного";

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
void MonitoringTestClass::ExpectAddTask(const size_t channelIndex,
                                        std::optional<CString> currentChannelName,
                                        std::optional<MonitoringInterval> currentInterval)
{
    TaskId taskId;
    if (!SUCCEEDED(CoCreateGuid(&taskId)))
        EXT_ASSERT(!"Не удалось создать гуид!");

    EXPECT_CALL(*m_monitoringServiceMock.get(), AddTaskList(_, Matcher(IMonitoringTasksService::eNormal))).
        WillOnce(Invoke(
            [taskId, chanName = std::move(currentChannelName), interval = std::move(currentInterval)]
    (const std::list<TaskParameters::Ptr>& listTaskParams, const IMonitoringTasksService::TaskPriority /*priority*/)
    {
        EXPECT_EQ(listTaskParams.size(), 1);
        if (chanName.has_value())
            EXPECT_STREQ(listTaskParams.front()->channelName, chanName.value()) << "Имя канала не совпало";
        if (interval.has_value())
            EXPECT_EQ(listTaskParams.front()->endTime - listTaskParams.front()->startTime, monitoring_interval_to_timespan(interval.value())) << "Интервал запроса данных не совпал";

        return taskId;
    }));

    ASSERT_TRUE(m_channelsData.size() > channelIndex);
    std::next(m_channelsData.begin(), channelIndex)->first = taskId;
}

void MonitoringTestClass::ExpectRemoveCurrentTask(const size_t channelIndex)
{
    ASSERT_TRUE(m_channelsData.size() > channelIndex);
    auto channelIt = std::next(m_channelsData.begin(), channelIndex);
    EXPECT_NE(channelIt->first, GUID_NULL);
    EXPECT_CALL(*m_monitoringServiceMock.get(), RemoveTask(TaskIdComporator(channelIt->first))).Times(1);
    channelIt->first = GUID_NULL;
}

//----------------------------------------------------------------------------//
void MonitoringTestClass::checkModelAndRealChannelsData(const std::string& testDescr)
{
    auto reportText = [&testDescr](const size_t index, const std::string& extraText)
    {
        CStringA resText;
        resText = std::string_swprintf"%s: Различаются данные по каналу №%u, различаются %s.", testDescr.c_str(), index, extraText.c_str());
        return resText;
    };

    // индекс проверяемого канала
    size_t index = 0;
    // проверяем что у локального списка каналов данные совпадают
    for (auto&& [taskId, modelChannelData] : m_channelsData)
    {
        const MonitoringChannelData& serviceChannelData = m_monitoringService->GetMonitoringChannelData(index);

        EXPECT_EQ(modelChannelData.bNotify, serviceChannelData.bNotify) << reportText(index, "флаг включенности оповещений");
        EXPECT_STREQ(modelChannelData.channelName, serviceChannelData.channelName) << reportText(index, "названия каналов");
        EXPECT_EQ(modelChannelData.monitoringInterval, serviceChannelData.monitoringInterval) << reportText(index, "интервалы мониторинга");

        if (isfinite(modelChannelData.alarmingValue) && isfinite(serviceChannelData.alarmingValue))
            // если они не наны - сравниваем числа
            EXPECT_FLOAT_EQ(modelChannelData.alarmingValue, serviceChannelData.alarmingValue) << reportText(index, "оповещательное значение");
        else
            // если кто-то нан то второй тоже должен быть наном
            EXPECT_EQ(isfinite(modelChannelData.alarmingValue), isfinite(serviceChannelData.alarmingValue)) << reportText(index, "оповещательное значение");

        ++index;
    }

    EXPECT_EQ(m_monitoringService->GetNumberOfMonitoringChannels(), m_channelsData.size()) << testDescr + ": Различаются количество каналов.";
}

//----------------------------------------------------------------------------//
void MonitoringTestClass::testAddChannels()
{
    const CString defaultChannelName = *m_monitoringService->GetNamesOfAllChannels().begin();
    const MonitoringInterval defaultMonitoringInterval = MonitoringChannelData().monitoringInterval;

    // добавляем в списорк все каналы которые можем
    for (size_t ind = 0; ind < kMonitoringChannelsCount; ++ind)
    {
        m_channelsData.emplace_back().second.channelName = L"Прогибометр №1";

        ExpectAddTask(ind, defaultChannelName, defaultMonitoringInterval);
        const size_t res = ExpectNotificationAboutListChangesWithReturn(&ITrendMonitoring::AddMonitoringChannel);
        EXPECT_EQ(res, ind) << "После добавления в списке каналов почему-то больше каналов чем должно";
    }

    // проверяем что параметры совпали
    checkModelAndRealChannelsData("Добавление каналов");
}

//----------------------------------------------------------------------------//
void MonitoringTestClass::testSetParamsToChannels()
{
    auto applyModelSettingsToChannel = [&](const size_t channelIndex, const MonitoringChannelData& modelChannelData)
    {
        ExpectNotificationAboutListChanges(&ITrendMonitoring::ChangeMonitoringChannelNotify,        channelIndex, modelChannelData.bNotify);
        ExpectNotificationAboutListChanges(&ITrendMonitoring::ChangeMonitoringChannelAlarmingValue, channelIndex, modelChannelData.alarmingValue);

        ExpectRemoveCurrentTask(channelIndex);
        ExpectAddTask(channelIndex, modelChannelData.channelName);
        ExpectNotificationAboutListChanges(&ITrendMonitoring::ChangeMonitoringChannelNotify,          channelIndex, modelChannelData.channelName);

        ExpectRemoveCurrentTask(channelIndex);
        ExpectAddTask(channelIndex, modelChannelData.channelName, modelChannelData.monitoringInterval);
        ExpectNotificationAboutListChanges(&ITrendMonitoring::ChangeMonitoringChannelInterval,      channelIndex, modelChannelData.monitoringInterval);
    };

    size_t index = 1;
    // меняем настройки среднего канала
    MonitoringChannelData* modelChannelData = &(std::next(m_channelsData.begin(), index)->second);
    modelChannelData->bNotify = false;
    modelChannelData->channelName = L"Прогибометр №2";
    modelChannelData->monitoringInterval = MonitoringInterval::eThreeMonths;
    modelChannelData->alarmingValue = -1;
    applyModelSettingsToChannel(index, *modelChannelData);

    index = kMonitoringChannelsCount - 1;
    // меняем настройки последнего канала
    modelChannelData = &(std::next(m_channelsData.begin(), index)->second);
    modelChannelData->bNotify = false;
    modelChannelData->channelName = L"Прогибометр №3";
    modelChannelData->monitoringInterval = MonitoringInterval::eOneDay;
    modelChannelData->alarmingValue = 100;
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
    size_t result = ExpectNotificationAboutListChangesWithReturn(&ITrendMonitoring::MoveUpMonitoringChannelByIndex, 1);
    EXPECT_EQ(result, 0) << "После перемещения конала вверх его индекс не валиден";
    m_channelsData.splice(m_channelsData.begin(), m_channelsData, ++m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Перемещение каналов в списке");

    // 0 Прогибометр 2      ↑
    // 1 Прогибометр 1
    // 2 Прогибометр 3
    result = ExpectNotificationAboutListChangesWithReturn(&ITrendMonitoring::MoveUpMonitoringChannelByIndex, 0);
    EXPECT_EQ(result, 2) << "После перемещения вверх первого канала он должен оказаться в конце";
    m_channelsData.splice(m_channelsData.end(), m_channelsData, m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Перемещение каналов в списке");

    // 0 Прогибометр 1
    // 1 Прогибометр 3      ↓
    // 2 Прогибометр 2
    result = ExpectNotificationAboutListChangesWithReturn(&ITrendMonitoring::MoveDownMonitoringChannelByIndex, 1);
    EXPECT_EQ(result, 2) << "После перемещения вниз среднего канала он должен оказаться последним";
    m_channelsData.splice(m_channelsData.end(), m_channelsData, ++m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Перемещение каналов в списке");

    // 0 Прогибометр 1
    // 1 Прогибометр 2
    // 2 Прогибометр 3      ↓
    result = ExpectNotificationAboutListChangesWithReturn(&ITrendMonitoring::MoveDownMonitoringChannelByIndex, 2);
    EXPECT_EQ(result, 0) << "После перемещения вниз последнего канала он должен оказаться первым";
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

    size_t channelIndex = 1;

    // удаление элементов
    ExpectRemoveCurrentTask(channelIndex);
    size_t result = ExpectNotificationAboutListChangesWithReturn(&ITrendMonitoring::RemoveMonitoringChannelByIndex, channelIndex);
    EXPECT_EQ(result, 1) << "После удаления канала выделенный индекс не корректнен";
    m_channelsData.erase(++m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Удаление каналов из списка");

    channelIndex = 0;
    ExpectRemoveCurrentTask(channelIndex);
    result = ExpectNotificationAboutListChangesWithReturn(&ITrendMonitoring::RemoveMonitoringChannelByIndex, channelIndex);
    EXPECT_EQ(result, 0) << "После удаления канала выделенный индекс не корректнен";
    m_channelsData.erase(m_channelsData.begin());
    // проверяем что у локального списка каналов данные совпадают
    checkModelAndRealChannelsData("Удаление каналов из списка");
}
