#include "pch.h"

#include <DirsService.h>

TEST(TestInit, InitializeTestInfo)
{
    // включает выдачу в аутпут русского текста
    setlocale(LC_ALL, "Russian");

    EXPECT_TRUE(AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0)) << "Ќе удалось инициализировать приложение";

    // задаем директорию с сигналами как тестовую директорию
    auto& zetDirsService = get_service<DirsService>();
    zetDirsService.setZetSignalsDir(zetDirsService.getExeDir() + LR"(Signals\)");
}