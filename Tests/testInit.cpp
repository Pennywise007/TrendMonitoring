#include "pch.h"

#include <DirsService.h>

#include "TrendMonitoringHandler.h"

TEST(TestInit, InitializeTestInfo)
{
    // включает выдачу в аутпут русского текста
    setlocale(LC_ALL, "Russian");

    EXPECT_TRUE(AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0)) << "Ќе удалось инициализировать приложение";

    // задаем директорию с сигналами как тестовую директорию
    auto& zetDirsService = get_service<DirsService>();
    zetDirsService.setZetSignalsDir(zetDirsService.getExeDir() + LR"(Signals\)");
    // только потому что список каналов грузитс€ из директории со сжатыми сигналами подмен€ем
    zetDirsService.setZetCompressedDir(zetDirsService.getExeDir() + LR"(Signals\)");

    // «апускаем сервис мониторинга чтобы он делал свои гр€зные дела перед другими тестами
    get_service<TrendMonitoringHandler>();
}