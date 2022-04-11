#include "pch.h"
#include "DependencyRegistration.h"

#include <tuple>
#include <shared_mutex>
#include <list>

#include <TelegramThread.h>

#include <ext/serialization/iserializable.h>

#include <ext/core/dependency_injection.h>
#include <ext/utils/call_once.h>

#include "include/IDirService.h"
#include "src/DirServiceImpl.h"

#include "include/IMonitoringTasksService.h"
#include "src/MonitoringTaskService/MonitoringTasksServiceImpl.h"

#include "include/ITrendMonitoring.h"
#include "src/TrendMonitoring.h"

#include "include/ITelegramBot.h"
#include "src/Telegram/TelegramBot.h"
#include "src/Telegram/TelegramSettings.h"
#include "src/Telegram/TelegramThreadImpl.h"

#include "include/ITelegramUsersList.h"

void DependencyRegistration::RegisterServices()
{
    CALL_ONCE(
    (
        ext::ServiceCollection& serviceCollection = ext::get_service<ext::ServiceCollection>();

        serviceCollection.RegisterSingleton<DirServiceImpl, IDirService>();

        serviceCollection.RegisterSingleton<TrendMonitoring, ITrendMonitoring>();
        serviceCollection.RegisterSingleton<MonitoringTasksServiceImpl, IMonitoringTasksService>();

        serviceCollection.RegisterSingleton<telegram::users::TelegramUsersList, telegram::users::ITelegramUsersList>();
        serviceCollection.RegisterSingleton<telegram::bot::TelegramParameters, telegram::bot::ITelegramBotSettings>();

        serviceCollection.RegisterSingleton<telegram::bot::CTelegramBot, telegram::bot::ITelegramBot>();
        serviceCollection.RegisterSingleton<telegram::thread::TelegramThreadImpl, ITelegramThread>();
    ));
}
