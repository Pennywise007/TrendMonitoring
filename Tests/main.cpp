#include "pch.h"

#include <afxwin.h>

#include <DirsService.h>

#include "TestHelper.h"

int main(int argc, char** argv)
{
    ::testing::FLAGS_gtest_catch_exceptions = false;

    ::testing::InitGoogleMock(&argc, argv);
    ::testing::InitGoogleTest(&argc, argv);

    // включает выдачу в аутпут русского текста
    setlocale(LC_ALL, "Russian");

    EXPECT_TRUE(AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0)) << "Не удалось инициализировать приложение";

    // задаем директорию с сигналами как тестовую директорию
    auto& zetDirsService = get_service<DirsService>();
    zetDirsService.setZetSignalsDir(zetDirsService.getExeDir() + LR"(Signals\)");
    // только потому что список каналов грузится из директории со сжатыми сигналами подменяем
    zetDirsService.setZetCompressedDir(zetDirsService.getExeDir() + LR"(Signals\)");

    const TestHelper& helper = get_service<TestHelper>();
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

    ////////////////////////////////////////////////////////////////////////////
    // запускаем тесты
    const int res = RUN_ALL_TESTS();
    ////////////////////////////////////////////////////////////////////////////

    // восстанавливаем сохраненный конфиг или удаляем созданный тестом
    if (copyConfigFilePath.has_value() && std::filesystem::is_regular_file(copyConfigFilePath.value()))
        // если был реальный файл конфига сохранён копией - возвращаем его на место
        std::filesystem::rename(copyConfigFilePath.value(), currentConfigPath);
    else
    {
        // подчищаем созданный файл с настройками
        EXPECT_TRUE(std::filesystem::is_regular_file(currentConfigPath)) << "Файл с настройками мониторинга не был создан!";
        EXPECT_TRUE(std::filesystem::remove(currentConfigPath)) << "Не удалось удалить файл";
    }

    // восстанавливаем файл рестарта
    if (copyRestartFilePath.has_value() && std::filesystem::is_regular_file(copyRestartFilePath.value()))
        // если был реальный файл конфига сохранён копией - возвращаем его на место
        std::filesystem::rename(copyRestartFilePath.value(), restartFilePath);
    else
    {
        // файл рестарта должен был быть удалён после использования в тестах
        EXPECT_FALSE(std::filesystem::is_regular_file(restartFilePath)) << "Файл рестарта не был удалён!";
    }

    return res;
}