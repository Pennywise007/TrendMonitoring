// вспомогательный файл для работы с образцами

#include "pch.h"
#include "resource.h"

#include <filesystem>
#include <fstream>


///////////////////////////////////////////////////////////////////////////////
// Вспомогательны класс для блокировки и загрузки ресурсов
struct ResourceLocker
{
    ResourceLocker(HGLOBAL hLoadedResource)
    {
        m_hLoadedResource = hLoadedResource;
        // захватываем ресурсы
        m_pLockedResource = ::LockResource(hLoadedResource);
    }
    // освобождаем ресурсы
    ~ResourceLocker() { ::FreeResource(m_hLoadedResource); }

    LPVOID m_pLockedResource;
    HGLOBAL m_hLoadedResource;
};

//****************************************************************************//
// Функция сравнения файла с ресурсным файлом
void compareWithResourceFile(const CString& verifiableFile,
                             const UINT resourceId, const CString& resourceName)
{
    std::ifstream file(verifiableFile, std::ifstream::binary | std::ifstream::ate);
    ASSERT_FALSE(file.fail()) << "Проблема при открытии файла " + CStringA(verifiableFile);

    // грузим ресурсный файл
    HRSRC hResource = ::FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(resourceId), resourceName);
    ASSERT_TRUE(hResource) << "Ошибка при загрузке ресурса " + CStringA(resourceName);
    HGLOBAL hGlob = LoadResource(GetModuleHandle(NULL), hResource);
    ASSERT_TRUE(hGlob) << "Ошибка при загрузке ресурса " + CStringA(resourceName);

    // лочим ресурсы и получаем размер данных
    ResourceLocker resourceLocker(hGlob);
    DWORD dwResourceSize = ::SizeofResource(GetModuleHandle(NULL), hResource);
    ASSERT_TRUE(resourceLocker.m_pLockedResource && dwResourceSize != 0) << "Ошибка при загрузке ресурса " + CStringA(resourceName);

    // проверяем что размеры совпадают
    ASSERT_EQ(file.tellg(), dwResourceSize) <<
        "Размер файла " + CStringA(verifiableFile) + " и ресурса " + CStringA(resourceName) + " отличается";

    // перемещаемся в начало и будем сравнивать содержимое
    file.seekg(0, std::ifstream::beg);

    // выгружаем содержимое файла в строку
    std::string fileText;
    std::copy(std::istreambuf_iterator<char>(file),
              std::istreambuf_iterator<char>(),
              std::insert_iterator<std::string>(fileText, fileText.begin()));

    // сравниваем содержимое ресурса и файла
    ASSERT_EQ(fileText, (char*)resourceLocker.m_pLockedResource) <<
        "Содержимое файла " + CStringA(verifiableFile) + " и ресурса " + CStringA(resourceName) + " отличается";
}