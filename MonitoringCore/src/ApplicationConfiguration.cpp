#include "pch.h"

#include "ApplicationConfiguration.h"

ChannelParameters::ChannelParameters()
{
    REGISTER_SERIALIZABLE_OBJECT(channelName);
    REGISTER_SERIALIZABLE_OBJECT(bNotify);
    REGISTER_SERIALIZABLE_OBJECT(monitoringInterval);
    REGISTER_SERIALIZABLE_OBJECT(alarmingValue);
}

ChannelParameters::ChannelParameters(const std::wstring& initChannelName)
{
    channelName = initChannelName;

    REGISTER_SERIALIZABLE_OBJECT(channelName);
    REGISTER_SERIALIZABLE_OBJECT(bNotify);
    REGISTER_SERIALIZABLE_OBJECT(monitoringInterval);
    REGISTER_SERIALIZABLE_OBJECT(alarmingValue);
}

const MonitoringChannelData& ChannelParameters::GetMonitoringData() const
{
    return *this;
}

void ChannelParameters::SetTrendChannelData(const TrendChannelData& data)
{
    trendData = data;
}

bool ChannelParameters::ChangeName(const std::wstring& newName)
{
    if (channelName == newName)
        return false;

    channelName = newName;
    ResetChannelData();
    return true;
}

bool ChannelParameters::ChangeNotification(const bool state)
{
    if (bNotify == state)
        return false;

    bNotify = state;
    return true;
}

bool ChannelParameters::ChangeInterval(const MonitoringInterval newInterval)
{
    if (monitoringInterval == newInterval)
        return false;

    monitoringInterval = newInterval;
    ResetChannelData();
    return true;
}

bool ChannelParameters::ChangeAlarmingValue(const float newvalue)
{
    if (alarmingValue == newvalue)
        return false;

    alarmingValue = newvalue;
    return true;
}

void ChannelParameters::ResetChannelData()
{
    // reset the state of data load on the channel
    channelState.dataLoaded = false;
    channelState.loadingDataError = false;

    m_loadingParametersIntervalEnd.reset();
}
