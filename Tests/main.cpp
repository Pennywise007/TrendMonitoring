#include "pch.h"

#include <DirsService.h>

#include "TestHelper.h"

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    // включает выдачу в аутпут русского текста
    setlocale(LC_ALL, "Russian");

    EXPECT_TRUE(AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0)) << "Не удалось инициализировать приложение";

    // задаем директорию с сигналами как тестовую директорию
    auto& zetDirsService = get_service<DirsService>();
    zetDirsService.setZetSignalsDir(zetDirsService.getExeDir() + LR"(Signals\)");
    // только потому что список каналов грузится из директории со сжатыми сигналами подменяем
    zetDirsService.setZetCompressedDir(zetDirsService.getExeDir() + LR"(Signals\)");

    TestHelper& handler = get_service<TestHelper>();
    // пока мы будем делать тесты могут попортиться реальные конфиги, если они есть сохраняем их и потом вернём
    const std::filesystem::path currentConfigPath(handler.getConfigFilePath());
    const std::filesystem::path copyConfigFilePath(handler.getCopyConfigFilePath());
    if (std::filesystem::is_regular_file(currentConfigPath))
        std::filesystem::rename(currentConfigPath, copyConfigFilePath);

    // делаем то же самое с файлом рестарта системы
    const std::filesystem::path restartFilePath(handler.getRestartFilePath());
    const std::filesystem::path copyRestartFilePath(handler.getCopyConfigFilePath());
    if (std::filesystem::is_regular_file(restartFilePath))
        std::filesystem::rename(restartFilePath, copyRestartFilePath);

    ////////////////////////////////////////////////////////////////////////////
    // запускаем тесты
    int res = RUN_ALL_TESTS();
    ////////////////////////////////////////////////////////////////////////////

    // восстанавливаем сохраненный конфиг или удаляем созданный тестом
    if (std::filesystem::is_regular_file(copyConfigFilePath))
        // если был реальный файл конфига сохранён копией - возвращаем его на место
        std::filesystem::rename(copyConfigFilePath, currentConfigPath);
    else
    {
        // подчищаем созданный файл с настройками
        EXPECT_TRUE(std::filesystem::is_regular_file(currentConfigPath)) << "Файл с настройками мониторинга не был создан!";
        EXPECT_TRUE(std::filesystem::remove(currentConfigPath)) << "Не удалось удалить файл";
    }

    // восстанавливаем файл рестарта
    if (std::filesystem::is_regular_file(copyRestartFilePath))
        // если был реальный файл конфига сохранён копией - возвращаем его на место
        std::filesystem::rename(copyRestartFilePath, restartFilePath);
    else
    {
        // подчищаем созданный файл рестарта
        EXPECT_TRUE(std::filesystem::is_regular_file(restartFilePath)) << "Файл с настройками мониторинга не был создан!";
        EXPECT_TRUE(std::filesystem::remove(restartFilePath)) << "Не удалось удалить файл";
    }

    return res;
}