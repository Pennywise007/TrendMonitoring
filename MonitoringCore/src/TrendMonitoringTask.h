#pragma once

#include <include/IMonitoringTasksService.h>

#include "ApplicationConfiguration.h"

// структура с параметрами задани€
struct MonitoringTaskInfo
{
    // тип выполн€емого задани€
    enum class TaskType
    {
        eIntervalInfo = 0,  // «апрос данных за указанный интервал(с перезаписью)
        eUpdatingInfo,      // «апрос новых данных(обновление существующей информации)
        eEveryDayReport     // «апрос данных дл€ ежедневного отчЄта
    } taskType = TaskType::eIntervalInfo;

    // параметры каналов по которым выполен€етс€ задание
    ChannelParametersList channelParameters;
};

struct MonitoringTaskResultHandler
{
    // ќбработка результатов мониторинга дл€ канала
    // @param monitoringResult - результат мониторинга
    // @param channelParameters - параметры канала
    // @param alertText - текст о котором нужно сообщить
    // @return true - в случае необходимости обновить данные по каналам
    static bool HandleIntervalInfoResult(const MonitoringTaskInfo::TaskType taskType,
                                         const IMonitoringTaskEvent::ResultDataPtr& monitoringResult,
                                         ChannelParameters* channelParameters,
                                         std::wstring& alertText);
};
