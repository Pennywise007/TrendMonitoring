// Проверка COM объектов, их времени жизни и доступности

#include "pch.h"

#include <COM.h>

DECLARE_COM_INTERFACE(ITest, "A740AEFD-B160-447C-8771-43E29D8310DD")
{
    virtual int getVal() = 0;
};

DECLARE_COM_INTERFACE(ITestSecond, "1F42CF13-9B8A-47DE-B75F-43CB0D215E7A")
{
};

DECLARE_COM_INTERFACE_(ITestWithBase, ITest, "16400C3E-C5DA-4C69-BA84-6F68F871ACF2")
{
};

// Наследует и реализует ITest, интерфейс прописан в COM_MAP
DECLARE_COM_BASE_CLASS(TestBaseClass)
    , public ITest
{
public:
    TestBaseClass() = default;
    TestBaseClass(int a) { val = a; }

    BEGIN_COM_MAP(TestBaseClass)
        COM_INTERFACE_ENTRY(ITest)
    END_COM_MAP()

    static const int kValue = -10;

// ITest
    virtual int getVal() override { return val; }

    int val = kValue;
};
// Наследует только ITest, интерфейс прописан в COM_MAP
DECLARE_COM_INTERNAL_CLASS(TestClass)
    , public ITest
{
public:
    TestClass() = default;
    TestClass(int a) { val = a; }

    BEGIN_COM_MAP(TestClass)
        COM_INTERFACE_ENTRY(ITest)
    END_COM_MAP()

    static const int kValue = 101;

// ITest
    int getVal() override { return val; }

    int val = kValue;
};
// Наследует ITestSecond и TestBaseClass, передает COM_MAP базовому классу
DECLARE_COM_INTERNAL_CLASS_(TestClassSecond, TestBaseClass)
    , public ITestSecond
{
public:
    TestClassSecond() { val = kValue; }
    TestClassSecond(int a) : TestBaseClass(a) { val = a; }

    BEGIN_COM_MAP(TestClassSecond)
        COM_INTERFACE_ENTRY(ITestSecond)
        COM_INTERFACE_ENTRY_CHAIN(TestBaseClass)
    END_COM_MAP()

    static const int kValue = 201;

// ITest
    int getVal() override { return val; }
};
// наследует TestBaseClass, но не передает ему COM_MAP
DECLARE_COM_INTERNAL_CLASS_(TestClassSecondNoChain, TestBaseClass)
    , public ITestSecond
{
public:
    TestClassSecondNoChain() = default;

    BEGIN_COM_MAP(TestClassSecondNoChain)
        COM_INTERFACE_ENTRY(ITestSecond)
    END_COM_MAP()

    static const int kValue = 70;

// ITest
    int getVal() override { return kValue; }
};
// наследует ITestWithBase, но ITest не прописан в COM_MAP
DECLARE_COM_INTERNAL_CLASS(TestClassInterfaceWithBase)
    , public ITestWithBase
{
public:
    TestClassInterfaceWithBase() = default;

    BEGIN_COM_MAP(TestClassInterfaceWithBase)
        COM_INTERFACE_ENTRY(ITestWithBase)
    END_COM_MAP()

    static const int kValue = 7;

// ITest
    int getVal() override { return kValue; }
};


// Проверка возможности создания COM классов
TEST(TestCOM, TestClassCreation)
{
    // класс наследник от ITest
    TestClass::Ptr pTestClass;
    // класс который наследуется от TestBaseClass
    TestClassSecond::Ptr pTestSecondClass;
    // класс который наследуется от TestBaseClass но не передает ему COM_INTERFACE_ENTRY_CHAIN
    TestClassSecondNoChain::Ptr pTestClassSecondNoChain;
    // класс который наследуется от интерфейса с наследованием
    TestClassInterfaceWithBase::Ptr pTestClassInterfaceWithBase;

    pTestClass = TestClass::create();
    EXPECT_FALSE(!pTestClass || pTestClass->getVal() != TestClass::kValue) << "Не удалось создать класс через ::create()";
    pTestClass = TestClass::make(10);
    EXPECT_FALSE(!pTestClass || pTestClass->getVal() != 10) << "Не удалось создать класс через ::make()";

    // создаем класс который наследуется от TestBaseClass
    pTestSecondClass = TestClassSecond::create();
    EXPECT_FALSE(!pTestSecondClass || pTestSecondClass->getVal() != TestClassSecond::kValue) << "Не удалось создать класс с наследием";

    // создаем класс который наследуется от TestBaseClass
    pTestSecondClass = TestClassSecond::make(110);
    EXPECT_FALSE(!pTestSecondClass || pTestSecondClass->getVal() != 110) << "Не удалось создать класс с наследием";

    // создаем класс который наследуется от TestBaseClass
    pTestClassSecondNoChain = TestClassSecondNoChain::create();
    EXPECT_FALSE(!pTestClassSecondNoChain ||
                 pTestClassSecondNoChain->getVal() != TestClassSecondNoChain::kValue) << "Не удалось создать класс с наследием без COM_INTERFACE_ENTRY_CHAIN";

    // создаем класс который наследуется от интерфейса с наследованием
    pTestClassInterfaceWithBase = TestClassInterfaceWithBase::create();
    EXPECT_FALSE(!pTestClassInterfaceWithBase ||
                 pTestClassInterfaceWithBase->getVal() != TestClassInterfaceWithBase::kValue) << "Не удалось создать класс от интерфейса с наследованием";
}

// Проверка возможности привести классы к (не)наследуемым интерфейсам
TEST(TestCOM, TestClassToInterfaceQuery)
{
    // интерфейсы
    ITestPtr pITest;
    ITestSecondPtr pITestSecond;
    ITestWithBasePtr pITestWithBase;

    pITest = TestClass::create();
    EXPECT_FALSE(!pITest || pITest->getVal() != TestClass::kValue) << "Не удалось привести класс к наследуемому интерфейсу";
    pITest = TestClassSecond::create();
    EXPECT_FALSE(!pITest || pITest->getVal() != TestClassSecond::kValue) << "Не удалось привести класс к наследуемому интерфейсу";
    pITest = TestClassSecondNoChain::create();
    EXPECT_FALSE(pITest) << "Удалось привести класс к интерфейсу которого нет в COM_MAP";
    pITest = TestClassInterfaceWithBase::create();
    EXPECT_FALSE(pITest) << "Удалось привести класс к интерфейсу которого нет в COM_MAP";

    pITestSecond = TestClass::create();
    EXPECT_FALSE(pITestSecond) << "Удалось привести класс к не наследуемому интерфейсу";
    pITestSecond = TestClassSecond::create();
    EXPECT_TRUE(pITestSecond) << "Не удалось привести класс к наследуемому интерфейсу";
    pITestSecond = TestClassSecondNoChain::create();
    EXPECT_TRUE(pITestSecond) << "Не удалось привести класс к наследуемому интерфейсу";
    pITestSecond = TestClassInterfaceWithBase::create();
    EXPECT_FALSE(pITestSecond) << "Удалось привести класс к не наследуемому интерфейсу";

    pITestWithBase = TestClass::create();
    EXPECT_FALSE(pITestWithBase) << "Удалось привести класс к не наследуемому интерфейсу";
    pITestWithBase = TestClassInterfaceWithBase::create();
    EXPECT_TRUE(pITestWithBase) << "Не удалось привести класс к интерфейсу с наследованием";
}

// Проверка приведения и приравнивания интерфейсов
TEST(TestCOM, TestInterfaceQuerying)
{
    // интерфейсы
    ITestPtr pITest;

    CComPtr<ITest> pCComITest;
    CComQIPtr<ITest> pCComQIITest;

    {
        pITest = TestClass::create();

        // операторы = к COM объектам от одного интерфейса
        pCComITest = pITest;
        EXPECT_TRUE(pCComITest) << "Не удалось приравнять интерфейс к CComPtr";
        pCComQIITest = pITest;
        EXPECT_TRUE(pCComITest) << "Не удалось приравнять интерфейс к CComQIPtr";
        pITest = pCComITest;
        EXPECT_TRUE(pITest) << "Не удалось приравнять CComPtr к интерфейсу";
        pITest = pCComQIITest;
        EXPECT_TRUE(pITest) << "Не удалось приравнять CComQIPtr к интерфейсу";
    }

    {
        pITest = TestClass::create();

        // конструкторы
        CComPtr<ITest> pCComITest(pITest);
        EXPECT_TRUE(pCComITest) << "Не удалось передать интерфейс в конструктор CComPtr";
        CComQIPtr<ITest> pCComQIITest(pITest);
        EXPECT_TRUE(pCComQIITest) << "Не удалось передать интерфейс в конструктор CComQIPtr";
        ITestPtr pITest1(pCComITest);
        EXPECT_TRUE(pITest1) << "Не удалось передать CComPtr в конструктор интерфейса";
        ITestPtr pITest2(pCComQIITest);
        EXPECT_TRUE(pITest2) << "Не удалось передать CComQIPtr в конструктор интерфейса";
    }

    {
        // операторы = к COM объектам от разных интерфейсов

        ITestSecondPtr pITestSecond = TestClassSecond::create();

        pITest = pITestSecond;
        EXPECT_TRUE(pITest) << "Не удалось приравнять ITestSecondPtr к ITestPtr";
        pCComITest = pITestSecond;
        EXPECT_TRUE(pCComITest) << "Не удалось приравнять ITestSecondPtr к CComPtr<ITest>";
        pCComQIITest = pITestSecond;
        EXPECT_TRUE(pCComQIITest) << "Не удалось приравнять ITestSecondPtr к CComQIPtr<ITest>";
        pITestSecond = pCComITest;
        EXPECT_TRUE(pITestSecond) << "Не удалось приравнять CComPtr<ITest> к ITestSecondPtr";
        pITestSecond = pCComQIITest;
        EXPECT_TRUE(pITestSecond) << "Не удалось приравнять CComQIPtr<ITest> к ITestSecondPtr";
    }

    {
        // конструкторы к COM объектам от разных интерфейсов

        ITestSecondPtr pITestSecond = TestClassSecond::create();

        ITestPtr pITest(pITestSecond);
        EXPECT_TRUE(pITest) << "Не удалось передать ITestSecondPtr в конструктор ITestPtr";
        CComPtr<ITest> pCComITest(pITestSecond);
        EXPECT_TRUE(pCComITest) << "Не удалось передать ITestSecondPtr в конструктор CComPtr<ITest>";
        CComQIPtr<ITest> pCComQIITest(pITestSecond);
        EXPECT_TRUE(pCComQIITest) << "Не удалось передать ITestSecondPtr в конструктор CComQIPtr<ITest>";
        ITestSecondPtr pITestSecond1(pCComITest);
        EXPECT_TRUE(pITestSecond1) << "Не удалось передать CComPtr<ITest> в конструктор ITestSecondPtr";
        ITestSecondPtr pITestSecond2(pCComQIITest);
        EXPECT_TRUE(pITestSecond2) << "Не удалось передать CComQIPtr<ITest> в конструктор ITestSecondPtr";
    }

    {
        // приведение типов, просто проверка на сборку

        CComPtr<ITestSecond> pCComITestSecond = TestClassSecond::create();
        EXPECT_TRUE(pCComITestSecond) << "Не удалось получить CComPtr от ComInternal через конструктор";
        // у CComPtr проблемы с оператором = к другим интерфейсам, используем приведение
        pCComITestSecond = (decltype(pCComITestSecond))TestClassSecond::create();
        EXPECT_TRUE(pCComITestSecond) << "Не удалось получить CComPtr от ComInternal через =";

        // у CComQIPtr нет конструктора к COM объектам с другим интерфейсом, используем приведение
        CComQIPtr<ITestSecond> pCComQIITestSecond =
            (decltype(pCComQIITestSecond))TestClassSecond::create();
        EXPECT_TRUE(pCComITestSecond) << "Не удалось получить CComQIPtr от ComInternal через конструктор";
        pCComQIITestSecond = TestClassSecond::create();
        EXPECT_TRUE(pCComITestSecond) << "Не удалось получить CComQIPtr от ComInternal через =";
    }

    {
        // проверка времени жизни объекта

        ITestSecondPtr pITestSecond;
        EXPECT_FALSE(pITestSecond) << "После конструктора интерфейса - объект не пустой";

        {
            ITestPtr pITestInternal = TestClassSecond::create();
            pITestSecond = pITestInternal;
        }

        EXPECT_TRUE(pITestSecond) << "После выхода из области видимости интерфейса - объект уничтожился";
    }
}