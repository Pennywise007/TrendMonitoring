#include "pch.h"

#include <afxwin.h>

#include "helpers/TestHelper.h"

#include "DependencyRegistration.h"

#include <ext/core.h>
#include <ext/core/dependency_injection.h>
#include <ext/std/filesystem.h>

#include "mocks/DirServiceMock.h"

#include "tests_telegram/TestTelegramBot.h"

using namespace testing;

int main(int argc, char** argv)
{
    ::testing::FLAGS_gtest_catch_exceptions = false;

    ::testing::InitGoogleMock(&argc, argv);
    ::testing::InitGoogleTest(&argc, argv);

    ext::core::Init();

    DependencyRegistration::RegisterServices();

    ext::ServiceCollection& serviceCollection = ext::get_service<ext::ServiceCollection>();
    serviceCollection.RegisterSingleton<DirServiceMock, IDirService, DirServiceMock>();
    serviceCollection.RegisterScoped<telegram::bot::TestTelegramThread, ITelegramThread, telegram::bot::TestTelegramThread>();
    serviceCollection.RegisterScoped<telegram::users::TestTelegramUsersList, telegram::users::ITelegramUsersList, telegram::users::TestTelegramUsersList>();

    const TestHelper& helper = ext::get_service<TestHelper>();
    // пока мы будем делать тесты могут попортиться реальные конфиги, если они есть сохраняем их и потом вернём
    const std::filesystem::path currentConfigPath(helper.getConfigFilePath());
    std::optional<std::filesystem::path> copyConfigFilePath(helper.getCopyConfigFilePath());
    if (std::filesystem::is_regular_file(currentConfigPath))
        std::filesystem::rename(currentConfigPath, copyConfigFilePath.value());
    else
        copyConfigFilePath.reset();

    // делаем то же самое с файлом рестарта системы
    const std::filesystem::path restartFilePath(helper.getRestartFilePath());
    std::optional<std::filesystem::path> copyRestartFilePath(helper.getCopyRestartFilePath());
    if (std::filesystem::is_regular_file(restartFilePath))
        std::filesystem::rename(restartFilePath, copyRestartFilePath.value());
    else
        copyRestartFilePath.reset();

    helper.ResetAll();

    ////////////////////////////////////////////////////////////////////////////
    // запускаем тесты
    const int res = RUN_ALL_TESTS();
    ////////////////////////////////////////////////////////////////////////////

    // after executing tests congif files must be deleted
    EXPECT_FALSE(std::filesystem::is_regular_file(currentConfigPath)) << "Файл с настройками мониторинга не был удалён!";
    EXPECT_FALSE(std::filesystem::is_regular_file(restartFilePath)) << "Файл рестарта не был удалён!";

    // восстанавливаем сохраненный конфиг или удаляем созданный тестом
    if (copyConfigFilePath.has_value() && std::filesystem::is_regular_file(copyConfigFilePath.value()))
        // если был реальный файл конфига сохранён копией - возвращаем его на место
        std::filesystem::rename(copyConfigFilePath.value(), currentConfigPath);

    // восстанавливаем файл рестарта
    if (copyRestartFilePath.has_value() && std::filesystem::is_regular_file(copyRestartFilePath.value()))
        // если был реальный файл конфига сохранён копией - возвращаем его на место
        std::filesystem::rename(copyRestartFilePath.value(), restartFilePath);

    return res;
}