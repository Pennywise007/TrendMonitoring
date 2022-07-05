// тестирование мониторнга данных, получение дautoанных и результатов

#include "pch.h"

#include "include/ChannelStateManager.h"

using namespace ::testing;


namespace testing {

inline void SetFirstErrorTime(ChannelStateManager& manager, CTime time)
{
    for (auto&& [errors, info] : manager.channelState)
    {
        info.m_timeOfFirstError = time;
        info.m_timeOfLastReporting = time;
    }
}

} // namespace testing

////////////////////////////////////////////////////////////////////////////////
// Тестирование задания и управления списком каналов
TEST(ChannelStateManagerTest, TestAddChannelErrorReport)
{
    ChannelStateManager manager;

    std::wstring errorMessage;

    const auto generateError = [&]()
    {
        manager.OnAddChannelErrorReport(ChannelStateManager::eFallenOff, errorMessage, L"Format тест");
    };

    for (int i = 0; i < 3; ++i)
    {
        generateError();
    }

    EXPECT_TRUE(errorMessage.empty()) << "First 3 calls should not generate reports";

    generateError();

    EXPECT_STREQ(errorMessage.c_str(), L"Format тест") << "3rd error should be sended";
    errorMessage.clear();
    for (int i = 0; i < 123; ++i)
    {
        generateError();
    }
    EXPECT_TRUE(errorMessage.empty()) << "We should suppress error for 3 hours after first sending";

    SetFirstErrorTime(manager, CTime::GetCurrentTime() - CTimeSpan(0, 5, 0, 0));
    generateError();
    EXPECT_STREQ(errorMessage.c_str(), L"В течениe 5 часов наблюдается ошибка: Format тест") << "Should send error";
    errorMessage.clear();

    for (int i = 0; i < 123; ++i)
    {
        generateError();
    }
    EXPECT_TRUE(errorMessage.empty()) << "We should suppress error for 3 hours after first sending";

    SetFirstErrorTime(manager, CTime::GetCurrentTime() - CTimeSpan(0, 7, 0, 0));
    generateError();
    EXPECT_STREQ(errorMessage.c_str(), L"В течениe 7 часов наблюдается ошибка: Format тест") << "Should send error";
    errorMessage.clear();

    manager.OnRemoveChannelErrorReport(ChannelStateManager::eFallenOff, errorMessage, L"Data ok 2");
    EXPECT_STREQ(errorMessage.c_str(), L"Data ok 2") << "Should send information about data normalization";

    errorMessage.clear();
    for (int i = 0; i < 123; ++i)
    {
        manager.OnRemoveChannelErrorReport(ChannelStateManager::eFallenOff, errorMessage, L"Data ok 2.4");
    }
    EXPECT_TRUE(errorMessage.empty()) << "We should not send extra normalization reports";
}

TEST(ChannelStateManagerTest, TestDataLost)
{
    ChannelStateManager manager;

    std::wstring errorMessage;
    manager.OnAddChannelErrorReport(ChannelStateManager::eLotOfEmptyData, errorMessage, L"Add lot of empty data text");
    EXPECT_FALSE(errorMessage.empty()) << "Should report about data loss";

    errorMessage.clear();
    manager.OnAddChannelErrorReport(ChannelStateManager::eFallenOff, errorMessage, L"Add fallen off text");

    manager.OnRemoveChannelErrorReport(ChannelStateManager::eFallenOff, errorMessage, L"Delete text");
    EXPECT_STREQ(errorMessage.c_str(), L"");
}
