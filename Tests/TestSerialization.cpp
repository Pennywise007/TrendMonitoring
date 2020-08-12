// �������� ��������������� ��������

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
// ������� ��������� ����� � ����� �� ��������
void compareWithResourceFile(const CString& verifiableFile,
                             const UINT resourceId, const CString& resourceName);


//****************************************************************************//
TEST(Serialization, TestXMLFileValid)
{
    // ���� XML
    CString fileXML = L"TestXMLFileValid.xml";

    // ������������

    // C����������� � XML
    ISerializator::Ptr pXMLSerializator = SerializationFabric::createXMLSerializator(fileXML);
    ASSERT_TRUE(pXMLSerializator) << "�� ������� ������� ������������";

    // ������������� �����
    TestSerializing::Ptr pSerializableClass = TestSerializing::create();
    ASSERT_TRUE(pSerializableClass) << "�� ������� ������� ������������� �����";

    // ����������� ����� � XML
    ASSERT_TRUE(SerializationExecutor::serializeObject(pXMLSerializator, pSerializableClass)) <<
        "�� ������� ������������� ������";

    // ��������� ��� ����� ������������ ��������� ���� ��������� � ����������� ��������
    compareWithResourceFile(fileXML, IDR_TESTXMLFILEVALID1, L"TestXMLFileValid");

    // ������� ��������� ����� ������������ ����
    ASSERT_TRUE(std::filesystem::remove(std::filesystem::path(fileXML.GetString()))) <<
        "�� ������� ������� ���� ����� ������������";
}

//****************************************************************************//
TEST(Serialization, TestXMLSerialization)
{
    // ���� XML
    CString fileXML = L"TestXMLSerialization.xml";

    // ������������

    // C����������� � XML
    ISerializator::Ptr pXMLSerializator = SerializationFabric::createXMLSerializator(fileXML);
    ASSERT_TRUE(pXMLSerializator) << "�� ������� ������� ������������";

    // ������������� �����
    TestSerializingClass::Ptr pSerializableClass = TestSerializingClass::create();
    ASSERT_TRUE(pSerializableClass) << "�� ������� ������� ������������� �����";
    // ��������� ����� ����������
    pSerializableClass->FillSerrializableValues();

    // ����������� ����� � XML
    ASSERT_TRUE(SerializationExecutor::serializeObject(pXMLSerializator, pSerializableClass)) <<
        "�� ������� ������������� ������";

    // ��������� ��� ����� ������������ ��������� ���� ��������� � ����������� ��������
    compareWithResourceFile(fileXML, IDR_TESTXMLSERIALIZATION1, L"TestXMLSerialization");

    // ��������������

    // C����������� � XML
    IDeserializator::Ptr pXMLDeserializator = SerializationFabric::createXMLDeserializator(fileXML);
    ASSERT_TRUE(pXMLDeserializator) << "�� ������� ������� ��������������";

    // ��������������� �����
    TestSerializingClass::Ptr pDeserializableClass = TestSerializingClass::create();
    ASSERT_TRUE(pDeserializableClass) << "�� ������� ������� ��������������� �����";

    // ������������� ����� �� XML
    ASSERT_TRUE(SerializationExecutor::deserializeObject(pXMLDeserializator, pDeserializableClass)) <<
                "�� ������� ��������������� ������";

    // ��������� ������� �� ������������ ���� �����
    EXPECT_EQ(pSerializableClass->m_iValue, pDeserializableClass->m_iValue) << "����� �������������� ������ �� �������!";
    EXPECT_EQ(pSerializableClass->m_lValue, pDeserializableClass->m_lValue) << "����� �������������� ������ �� �������!";
    EXPECT_EQ(pSerializableClass->m_bValue, pDeserializableClass->m_bValue) << "����� �������������� ������ �� �������!";
    EXPECT_EQ(pSerializableClass->m_listOfUINT, pDeserializableClass->m_listOfUINT) << "����� �������������� ������ �� �������!";
    EXPECT_EQ(pSerializableClass->m_vectorDoubleValues, pDeserializableClass->m_vectorDoubleValues) << "����� �������������� ������ �� �������!";
    EXPECT_EQ(pSerializableClass->m_mapStringTofloat, pDeserializableClass->m_mapStringTofloat) << "����� �������������� ������ �� �������!";

    // ��������� ������������ ����������� ������ m_serializingClass TestSerializing::Ptr
    checkProperties(*pSerializableClass->m_serializingClass, *pDeserializableClass->m_serializingClass);

    // ��������� ��� ������ ������������� ������� ��������� ����������������
    auto& serializableSubClass = pSerializableClass->m_serializingSubclass;
    auto& deserializableSubClass = pDeserializableClass->m_serializingSubclass;
    EXPECT_EQ(serializableSubClass.size(), deserializableSubClass.size()) <<
        "����� �������������� ������� �������� � ����������� �� �������!";

    auto serializableIt = serializableSubClass.begin();
    auto deserializableIt = deserializableSubClass.begin();
    for (size_t i = 0, size = std::min<size_t>(serializableSubClass.size(), deserializableSubClass.size());
         i < size; ++i, ++serializableIt, ++deserializableIt)
    {
        CStringA errorText;
        errorText.Format("������ � ������� ���������� �������� �� ������� %u", i);
        EXPECT_TRUE(CompareSubClasses(**serializableIt, **deserializableIt)) << errorText;
    }

    // ������� ��������� ����� ������������ ����
    ASSERT_TRUE(std::filesystem::remove(std::filesystem::path(fileXML.GetString()))) <<
        "�� ������� ������� ���� ����� ������������";
}

///////////////////////////////////////////////////////////////////////////////
// �������������� ����� ��� ���������� � �������� ��������
struct ResourceLocker
{
    ResourceLocker(HGLOBAL hLoadedResource)
    {
        m_hLoadedResource = hLoadedResource;
        // ����������� �������
        m_pLockedResource = ::LockResource(hLoadedResource);
    }
    // ����������� �������
    ~ResourceLocker() { ::FreeResource(m_hLoadedResource); }

    LPVOID m_pLockedResource;
    HGLOBAL m_hLoadedResource;
};

//****************************************************************************//
// ������� ��������� ����� � ��������� ������
void compareWithResourceFile(const CString& verifiableFile,
                             const UINT resourceId, const CString& resourceName)
{
    std::ifstream file(verifiableFile, std::ifstream::binary | std::ifstream::ate);
    ASSERT_FALSE(file.fail()) << "�������� ��� �������� ����� " + CStringA(verifiableFile);

    // ������ ��������� ����
    HRSRC hResource = ::FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(resourceId), resourceName);
    ASSERT_TRUE(hResource) << "������ ��� �������� ������� " + CStringA(resourceName);
    HGLOBAL hGlob = LoadResource(GetModuleHandle(NULL), hResource);
    ASSERT_TRUE(hGlob) << "������ ��� �������� ������� " + CStringA(resourceName);

    // ����� ������� � �������� ������ ������
    ResourceLocker resourceLocker(hGlob);
    DWORD dwResourceSize = ::SizeofResource(GetModuleHandle(NULL), hResource);
    ASSERT_TRUE(resourceLocker.m_pLockedResource && dwResourceSize != 0) << "������ ��� �������� ������� " + CStringA(resourceName);

    // ��������� ��� ������� ���������
    ASSERT_EQ(file.tellg(), dwResourceSize) <<
        "������ ����� " + CStringA(verifiableFile) + " � ������� " + CStringA(resourceName) + " ����������";

    // ������������ � ������ � ����� ���������� ����������
    file.seekg(0, std::ifstream::beg);

    // ��������� ���������� ����� � ������
    std::string fileText;
    std::copy(std::istreambuf_iterator<char>(file),
              std::istreambuf_iterator<char>(),
              std::insert_iterator<std::string>(fileText, fileText.begin()));

    // ���������� ���������� ������� � �����
    ASSERT_EQ(fileText, (char*)resourceLocker.m_pLockedResource) <<
        "���������� ����� " + CStringA(verifiableFile) + " � ������� " + CStringA(resourceName) + " ����������";
}