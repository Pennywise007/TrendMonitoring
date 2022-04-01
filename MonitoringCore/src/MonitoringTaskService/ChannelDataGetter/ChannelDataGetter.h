#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <set>
#include "ChannelsHelpHeader.h"

#include <ext/core/defines.h>

////////////////////////////////////////////////////////////////////////////////
// класс получения данных по определенному каналу за заданный промежуток времени
class ChannelDataGetter
{
public:
    ChannelDataGetter(const CString& channelName, const CTime& begin, const CTime& end, bool compressedDataMode = false);
    ChannelDataGetter(const CString& channelName, const CString& groupName, const CTime& begin, const CTime& end, bool compressedDataMode = false);
    virtual ~ChannelDataGetter() = default;

public:
    // получение директорий
    EXT_NODISCARD static std::wstring GetSignalsDir() EXT_THROWS(std::runtime_error);
    EXT_NODISCARD static std::wstring GetCompressedDir() EXT_THROWS(std::runtime_error);

    // получить данные по каналу
    void getSourceChannelData(const CTime& begin, const CTime& end, std::vector<float>& data, bool removeNAN = false);

    enum CompressedLevel
    {
        eSeconds1 = 0,
        eSeconds10,
        eMinutes1,
        eMinutes10,
        eHours1,
        eHours6,
        eDay,
    };
    // Get data from compressed dir by level
    void GetCompressedChannelData(CompressedLevel level, const CTime& begin, const CTime& end, std::vector<float>& pData);

    // удаляем выбросы из данных
    void deleteEmissionsValuesFromData(std::vector<float>& data) const;

    void DeleteEmptyData(std::vector<float>& data, float* averageVal = nullptr) const;

    // запонение списка каналов
    static void FillChannelList(std::list<CString>& channels, bool signals);
    static void FillChannelListWithGroupNames(std::list<std::pair<CString, CString>>& channelsWithGroupNames, bool signals);

    EXT_NODISCARD std::shared_ptr<ChannelInfo> getChannelInfo() const;

    void DeleteAllChannelData(const CTime& Begin, const CTime& End);

private:
    // функция удаления неподходящих под колбэк данных
    typedef std::function<bool(const float& value)> Callback;
    float deleteInappropriateValues(std::vector<float>& dataVect, Callback&& callback) const;

    // получение данных
    void calcChannelInfo(const CTime& begin, const CTime& end, bool compressionDir);

    // получение информации о канале
    bool getChannelInfo(const CString& xmlFileName) const;

    // получаем информацию о файлах которые необходимо переписать
    EXT_NODISCARD ChannelsFromFileByYear GetSourceFilesInfoByInterval(const CTime& Begin, const CTime& End);

    // плучить исходные данные канала на заданном интервале
    void GetSourceChannelDataByInterval(const CTime& tStart, const CTime& tEnd,
                                        long uiLength, float* pData);

    // get compressed files map
    void GetCompressedFilesMapOnInterval(CompressedLevel level, const CTime& tStart, const CTime& tEnd,
                                         ChannelsFromFileByYear& channelMap);
    // Get data from compressed dir by level
    void GetCompressedFilesDataOnInterval(CompressedLevel level, CTime tStart, CTime tEnd,
                                          std::vector<float>& pData);

private:
    std::shared_ptr<ChannelInfo> m_channelInfo;
};
