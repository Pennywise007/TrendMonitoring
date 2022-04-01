#pragma once

#include <afx.h>
#include <list>
#include <memory>
#include <optional>

#include <ext/core/dependency_injection.h>
#include <ext/core/noncopyable.h>

#include <ext/serialization/iserializable.h>

#include <include/ITrendMonitoring.h>

#include <include/ITelegramUsersList.h>
#include <include/ITelegramBot.h>

// Monitoring channel parameters
class ChannelParameters
    : public ext::serializable::SerializableObject<ChannelParameters>
    , public MonitoringChannelData
{
public:
    typedef std::shared_ptr<ChannelParameters> Ptr;

    ChannelParameters();
    ChannelParameters(const std::wstring& initChannelName);

public:
    /// <summary>Get data from channel.</summary>
    EXT_NODISCARD const MonitoringChannelData& GetMonitoringData() const;
    /// <summary>Set data for channel.</summary>
    void SetTrendChannelData(const TrendChannelData& data);
    /// <summary>Change channel name.</summary>
    /// <param name="newName">New channel name.</param>
    /// <returns>True if value changed.</returns>
    bool ChangeName(const std::wstring& newName);
    /// <summary>Change the notification flag about channel problems.</summary>
    /// <param name="state">New state.</param>
    /// <returns>True if value changed.</returns>
    bool ChangeNotification(const bool state);
    /// <summary>Change monitoring interval for channel.</summary>
    /// <param name="newInterval">new interval value.</param>
    /// <returns>True if value changed.</returns>
    bool ChangeInterval(const MonitoringInterval newInterval);
    /// <summary>Change notification value.</summary>
    /// <param name="newvalue">new value.</param>
    /// <returns>True if value changed.</returns>
    bool ChangeAlarmingValue(const float newValue);
    /// <summary>Reset memorized data by channel.</summary>
    void ResetChannelData();

public:
    // end of the time interval for which the parameters were loaded, if empty - data not loaded yet
    std::optional<CTime> m_loadingParametersIntervalEnd;
};

// List of channels and their parameters
typedef std::list<ChannelParameters::Ptr> ChannelParametersList;
typedef ChannelParametersList::iterator ChannelIt;

// All application settings
class ATL_NO_VTABLE ApplicationConfiguration
    : public ext::serializable::SerializableObject<ApplicationConfiguration>
    , ext::NonCopyable
{
public:
    explicit ApplicationConfiguration(ext::ServiceProvider::Ptr serviceProvider)
        : m_telegramParameters(ext::GetInterface<telegram::bot::ITelegramBotSettings>(serviceProvider))
    {
        REGISTER_SERIALIZABLE_OBJECT(m_telegramParameters);
    }

    typedef std::shared_ptr<ApplicationConfiguration> Ptr;

public:
    // list of settings for each channel
    DECLARE_SERIALIZABLE((ChannelParametersList) m_chanelParameters);

    // telegram bot settings
    std::shared_ptr<telegram::bot::ITelegramBotSettings> m_telegramParameters;
};
