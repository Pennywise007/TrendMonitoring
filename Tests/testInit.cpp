#include "pch.h"

#include <DirsService.h>

TEST(TestInit, InitializeTestInfo)
{
    // �������� ������ � ������ �������� ������
    setlocale(LC_ALL, "Russian");

    EXPECT_TRUE(AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0)) << "�� ������� ���������������� ����������";

    // ������ ���������� � ��������� ��� �������� ����������
    auto& zetDirsService = get_service<DirsService>();
    zetDirsService.setZetSignalsDir(zetDirsService.getExeDir() + LR"(Signals\)");
}