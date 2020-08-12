// Проверка сериализуемости объектов

#include "pch.h"
#include "resource.h"

#include <afx.h>
#include <filesystem>
#include <fstream>

#include <vector>
#include <list>
#include <map>

#include <COM.h>
#include "Serialization\SerializatorFabric.h"

#include "TestSerialization.h"

//****************************************************************************//
// Функция сравнения файла и файла из ресурсов
void compareWithResourceFile(const CString& verifiableFile,
                             const UINT resourceId, const CString& resourceName);


//****************************************************************************//
TEST(Serialization, TestXMLFileValid)
{
    // файл XML
    CString fileXML = L"TestXMLFileValid.xml";

    // Сериализация

    // Cериализатор в XML
    ISerializator::Ptr pXMLSerializator = SerializationFabric::createXMLSerializator(fileXML);
    ASSERT_TRUE(pXMLSerializator) << "Не удалось создать сериализатор";

    // Сериализуемый класс
    TestSerializing::Ptr pSerializableClass = TestSerializing::create();
    ASSERT_TRUE(pSerializableClass) << "Не удалось создать сериализуемый класс";

    // Сериализуем класс в XML
    ASSERT_TRUE(SerializationExecutor::serializeObject(pXMLSerializator, pSerializableClass)) <<
        "Не удалось сериализовать объект";

    // проверяем что после сериализации созданный файл совпадает с сохраненным образцом
    compareWithResourceFile(fileXML, IDR_TESTXMLFILEVALID1, L"TestXMLFileValid");

    // Удаляем созданный после сериализации файл
    ASSERT_TRUE(std::filesystem::remove(std::filesystem::path(fileXML.GetString()))) <<
        "Не удалось удалить файл после сериализации";
}

//****************************************************************************//
TEST(Serialization, TestXMLSerialization)
{
    // файл XML
    CString fileXML = L"TestXMLSerialization.xml";

    // Сериализация

    // Cериализатор в XML
    ISerializator::Ptr pXMLSerializator = SerializationFabric::createXMLSerializator(fileXML);
    ASSERT_TRUE(pXMLSerializator) << "Не удалось создать сериализатор";

    // Сериализуемый класс
    TestSerializingClass::Ptr pSerializableClass = TestSerializingClass::create();
    ASSERT_TRUE(pSerializableClass) << "Не удалось создать сериализуемый класс";
    // Заполняем класс значениями
    pSerializableClass->FillSerrializableValues();

    // Сериализуем класс в XML
    ASSERT_TRUE(SerializationExecutor::serializeObject(pXMLSerializator, pSerializableClass)) <<
        "Не удалось сериализовать объект";

    // проверяем что после сериализации созданный файл совпадает с сохраненным образцом
    compareWithResourceFile(fileXML, IDR_TESTXMLSERIALIZATION1, L"TestXMLSerialization");

    // Десериализация

    // Cериализатор в XML
    IDeserializator::Ptr pXMLDeserializator = SerializationFabric::createXMLDeserializator(fileXML);
    ASSERT_TRUE(pXMLDeserializator) << "Не удалось создать десериализатор";

    // Десериализуемый класс
    TestSerializingClass::Ptr pDeserializableClass = TestSerializingClass::create();
    ASSERT_TRUE(pDeserializableClass) << "Не удалось создать десериализуемый класс";

    // Десериализуем класс из XML
    ASSERT_TRUE(SerializationExecutor::deserializeObject(pXMLDeserializator, pDeserializableClass)) <<
                "Не удалось десериализовать объект";

    // Проверяем объекты на соответствие друг другу
    EXPECT_EQ(pSerializableClass->m_iValue, pDeserializableClass->m_iValue) << "После десериализации данные не совпали!";
    EXPECT_EQ(pSerializableClass->m_lValue, pDeserializableClass->m_lValue) << "После десериализации данные не совпали!";
    EXPECT_EQ(pSerializableClass->m_bValue, pDeserializableClass->m_bValue) << "После десериализации данные не совпали!";
    EXPECT_EQ(pSerializableClass->m_listOfUINT, pDeserializableClass->m_listOfUINT) << "После десериализации данные не совпали!";
    EXPECT_EQ(pSerializableClass->m_vectorDoubleValues, pDeserializableClass->m_vectorDoubleValues) << "После десериализации данные не совпали!";
    EXPECT_EQ(pSerializableClass->m_mapStringTofloat, pDeserializableClass->m_mapStringTofloat) << "После десериализации данные не совпали!";

    // проверяем правильность вычитывания класса m_serializingClass TestSerializing::Ptr
    checkProperties(*pSerializableClass->m_serializingClass, *pDeserializableClass->m_serializingClass);

    // проверяем что массив сериализуемых классов нормально десериализовался
    auto& serializableSubClass = pSerializableClass->m_serializingSubclass;
    auto& deserializableSubClass = pDeserializableClass->m_serializingSubclass;
    EXPECT_EQ(serializableSubClass.size(), deserializableSubClass.size()) <<
        "После десериализации размеры массивов с подклассами не совпали!";

    auto serializableIt = serializableSubClass.begin();
    auto deserializableIt = deserializableSubClass.begin();
    for (size_t i = 0, size = std::min<size_t>(serializableSubClass.size(), deserializableSubClass.size());
         i < size; ++i, ++serializableIt, ++deserializableIt)
    {
        CStringA errorText;
        errorText.Format("Данные в массиве подклассов различны по индексу %u", i);
        EXPECT_TRUE(CompareSubClasses(**serializableIt, **deserializableIt)) << errorText;
    }

    // Удаляем созданный после сериализации файл
    ASSERT_TRUE(std::filesystem::remove(std::filesystem::path(fileXML.GetString()))) <<
        "Не удалось удалить файл после сериализации";
}

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