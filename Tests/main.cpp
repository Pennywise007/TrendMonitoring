#include "pch.h"

#include <afxwin.h>

#include <DirsService.h>

#include "TestHelper.h"

int main(int argc, char** argv)
{
    ::testing::FLAGS_gtest_catch_exceptions = false;

    ::testing::InitGoogleMock(&argc, argv);
    ::testing::InitGoogleTest(&argc, argv);

    // �������� ������ � ������ �������� ������
    setlocale(LC_ALL, "Russian");

    EXPECT_TRUE(AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0)) << "�� ������� ���������������� ����������";

    // ������ ���������� � ��������� ��� �������� ����������
    auto& zetDirsService = get_service<DirsService>();
    zetDirsService.setZetSignalsDir(zetDirsService.getExeDir() + LR"(Signals\)");
    // ������ ������ ��� ������ ������� �������� �� ���������� �� ������� ��������� ���������
    zetDirsService.setZetCompressedDir(zetDirsService.getExeDir() + LR"(Signals\)");

    const TestHelper& helper = get_service<TestHelper>();
    // ���� �� ����� ������ ����� ����� ����������� �������� �������, ���� ��� ���� ��������� �� � ����� �����
    const std::filesystem::path currentConfigPath(helper.getConfigFilePath());
    std::optional<std::filesystem::path> copyConfigFilePath(helper.getCopyConfigFilePath());
    if (std::filesystem::is_regular_file(currentConfigPath))
        std::filesystem::rename(currentConfigPath, copyConfigFilePath.value());
    else
        copyConfigFilePath.reset();

    // ������ �� �� ����� � ������ �������� �������
    const std::filesystem::path restartFilePath(helper.getRestartFilePath());
    std::optional<std::filesystem::path> copyRestartFilePath(helper.getCopyRestartFilePath());
    if (std::filesystem::is_regular_file(restartFilePath))
        std::filesystem::rename(restartFilePath, copyRestartFilePath.value());
    else
        copyRestartFilePath.reset();

    ////////////////////////////////////////////////////////////////////////////
    // ��������� �����
    const int res = RUN_ALL_TESTS();
    ////////////////////////////////////////////////////////////////////////////

    // ��������������� ����������� ������ ��� ������� ��������� ������
    if (copyConfigFilePath.has_value() && std::filesystem::is_regular_file(copyConfigFilePath.value()))
        // ���� ��� �������� ���� ������� ������� ������ - ���������� ��� �� �����
        std::filesystem::rename(copyConfigFilePath.value(), currentConfigPath);
    else
    {
        // ��������� ��������� ���� � �����������
        EXPECT_TRUE(std::filesystem::is_regular_file(currentConfigPath)) << "���� � ����������� ����������� �� ��� ������!";
        EXPECT_TRUE(std::filesystem::remove(currentConfigPath)) << "�� ������� ������� ����";
    }

    // ��������������� ���� ��������
    if (copyRestartFilePath.has_value() && std::filesystem::is_regular_file(copyRestartFilePath.value()))
        // ���� ��� �������� ���� ������� ������� ������ - ���������� ��� �� �����
        std::filesystem::rename(copyRestartFilePath.value(), restartFilePath);
    else
    {
        // ���� �������� ������ ��� ���� ����� ����� ������������� � ������
        EXPECT_FALSE(std::filesystem::is_regular_file(restartFilePath)) << "���� �������� �� ��� �����!";
    }

    return res;
}