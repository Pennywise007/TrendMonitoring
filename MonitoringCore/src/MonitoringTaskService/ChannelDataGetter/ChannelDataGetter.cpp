#include "pch.h"

#include <afx.h>
#include <algorithm>
#include <functional>
#include <numeric>
#include <vector>

#include "ChannelDataGetter.h"

#include <pugixml.hpp>

#include <ext/std/filesystem.h>
#include <ext/std/string.h>
#include <ext/utils/registry.h>

using namespace pugi;

namespace {

EXT_NODISCARD ext::registry::Key get_setting_key() EXT_THROWS()
{
    return ext::registry::Key(L"SOFTWARE\\ZETLAB\\Settings", HKEY_LOCAL_MACHINE, KEY_READ | KEY_WOW64_64KEY);
}

DWORD get_allocation_granularity()
{
    //гранулярность для начального адреса, в котором может быть выделена виртуальная память
    const static DWORD dwAllocationGranularity = []()
    {
        SYSTEM_INFO sinf;
        GetSystemInfo(&sinf);
        return sinf.dwAllocationGranularity;
    }();
    return dwAllocationGranularity;
}

void get_channel_list(std::list<std::pair<CString, CString>>& channelsWithGroupNames, const wchar_t* sDirectory, bool withGroupNames)
{
    WIN32_FIND_DATA win32_find_data;
    WCHAR wcDirectory[MAX_PATH];
    wcscpy_s(wcDirectory, MAX_PATH, sDirectory);
    wcscat_s(wcDirectory, MAX_PATH, L"*.*");

    if (const HANDLE hFind = FindFirstFile(wcDirectory, &win32_find_data); hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(win32_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                if (std::wstring_view(win32_find_data.cFileName).find(L"sig0000.xml") != -1)
                {
                    xml_document hFile;
                    if (hFile.load_file(std::string_swprintf(L"%s%s", sDirectory, win32_find_data.cFileName).c_str(), parse_default, encoding_utf8))
                    {
                        if (const xml_node hRoot = hFile.child(L"CommonSignalDescriptor"); hRoot)
                        {
                            xml_object_range<xml_named_node_iterator> signals = hRoot.children(L"Signal");
                            for (const auto& signal : signals)
                            {
                                xml_attribute nameAtr = signal.attribute(L"name");
                                xml_attribute conversionAtr = signal.attribute(L"conversion");
                                xml_attribute groupName = signal.attribute(L"groupname");

                                if (nameAtr && conversionAtr && wcslen(nameAtr.value()) != 0 && wcslen(conversionAtr.value()) != 0)
                                {
                                    auto nameAndGroup = std::make_pair<CString, CString>(nameAtr.value(), groupName.value());
                                    if (std::find(channelsWithGroupNames.begin(), channelsWithGroupNames.end(), nameAndGroup) == channelsWithGroupNames.end())
                                        channelsWithGroupNames.emplace_back(std::move(nameAndGroup));
                                }
                            }

                            break;
                        }
                    }
                }
            }
            else if (std::wstring_view filename(win32_find_data.cFileName); filename != L"." && filename != L"..")
            {
                get_channel_list(channelsWithGroupNames, std::string_swprintf(L"%s%s\\", sDirectory, win32_find_data.cFileName).c_str(), withGroupNames);
            }
        } while (FindNextFile(hFind, &win32_find_data));
        FindClose(hFind);
    }
}

xml_node get_signal(xml_document& document, const std::shared_ptr<ChannelInfo>& channel)
{
    xml_node hRoot = document.child(L"CommonSignalDescriptor");
    if (hRoot != nullptr)
    {
        if (!channel->GetGroupName().IsEmpty())
        {
            auto pair = hRoot.children(L"Signal");
            for (auto& node : pair)
            {
                xml_attribute name = node.attribute(L"name");
                if (name && std::wstring_view(name.value()) == channel->GetName().GetString())
                {
                    xml_attribute groupName = node.attribute(L"groupname");
                    if (groupName && std::wstring_view(groupName.value()) == channel->GetGroupName().GetString())
                    {
                        return node;
                    }
                }
            }
            return {};
        }
        hRoot = hRoot.find_child_by_attribute(L"name", channel->GetName());
    }
    return hRoot;
}

}

//****************************************************************************//
ChannelDataGetter::ChannelDataGetter(const CString& channelName, const CTime& begin, const CTime& end, bool compressedDataMode)
    : m_channelInfo(new ChannelInfo())
{
    m_channelInfo->SetName(channelName);
    calcChannelInfo(begin, end, compressedDataMode);
}

ChannelDataGetter::ChannelDataGetter(const CString& channelName, const CString& groupName, const CTime& begin, const CTime& end, bool compressedDataMode)
    : m_channelInfo(new ChannelInfo())
{
    m_channelInfo->SetName(channelName);
    m_channelInfo->SetGroupName(groupName);
    calcChannelInfo(begin, end, compressedDataMode);
}

std::wstring ChannelDataGetter::GetSignalsDir()
{
    try
    {
        std::wstring result;
        EXT_CHECK(get_setting_key().GetRegistryValue(L"DirSignal", result));
        return result;
    }
    catch (...)
    {
        throw std::runtime_error(ext::ManageExceptionText("Не удалось найти директорию с сигналами в реестре. "
            "Ожидаемый путь в реестре: HKLM\\SOFTWARE\\ZETLAB\\Settings DirSignal"));
    }
}

std::wstring ChannelDataGetter::GetCompressedDir()
{
    try
    {
        std::wstring result;
        EXT_CHECK(get_setting_key().GetRegistryValue(L"DirCompressed", result));
        return result;
    }
    catch (...)
    {
        throw std::runtime_error(ext::ManageExceptionText("Не удалось найти директорию с сигналами в реестре. "
            "Ожидаемый путь в реестре: HKLM\\SOFTWARE\\ZETLAB\\Settings DirCompressed"));
    }
}

//----------------------------------------------------------------------------//
void ChannelDataGetter::getSourceChannelData(const CTime& timeStart, const CTime& timeEnd, std::vector<float>& data, bool removeNAN)
{
    // получаем инфу по каналу
    calcChannelInfo(timeStart, timeEnd, false);

    // вычисляем сколько надо данных
    const CTimeSpan tsDelta = timeEnd - timeStart;
    const double freq = m_channelInfo->GetFrequency();

    const int dataSize = int(tsDelta.GetTotalSeconds() * freq);

    if (tsDelta.GetTotalSeconds() <= 0)
        throw std::exception("Неверный временной интервал для загрузки исходных данных");

    data.resize(dataSize);
    // заполним все нулями после ресайза потому что буфер мог уже использоваться и если в данных пропуски
    // то данные будут невалидны
    std::fill(data.begin(), data.end(), removeNAN ? 0.f : NAN);

    GetSourceChannelDataByInterval(timeStart, timeEnd, dataSize, data.data());

    if (removeNAN)
        // заменяем наны на нули
        std::replace_if(data.begin(), data.end(),
                        [](auto val)
                        {
                            return _finite(val) == 0;
                        },
                        0.f);
}

void ChannelDataGetter::GetCompressedChannelData(CompressedLevel level, const CTime& begin, const CTime& end,
                                                 std::vector<float>& data)
{
    // получаем инфу по каналу
    calcChannelInfo(begin, end, true);

    if (CTimeSpan(end - begin).GetTotalSeconds() <= 0)
        throw std::exception("Неверный временной интервал для загрузки исходных данных");

    GetCompressedFilesDataOnInterval(level, begin, end, data);

    // заменяем наны на нули
    std::replace_if(data.begin(), data.end(),
                    [](auto val)
                    {
                        return _finite(val) == 0;
                    },
                    0.f);
}

//----------------------------------------------------------------------------//
void ChannelDataGetter::deleteEmissionsValuesFromData(std::vector<float>& data) const
{
    // удаляем нули и наны
    float averageVal;
    DeleteEmptyData(data, &averageVal);

    const size_t dataSize = data.size();
    if (dataSize > 10)
    {
        // https://ru.m.wikihow.com/%D0%B2%D1%8B%D1%87%D0%B8%D1%81%D0%BB%D0%B8%D1%82%D1%8C-%D0%B2%D1%8B%D0%B1%D1%80%D0%BE%D1%81%D1%8B

        //копируем полученные данные, т.к будем их сортировать
        std::vector<float> copyDataVect = data;
        // соритруем
        std::sort(copyDataVect.begin(), copyDataVect.end());

        // функция вычисления медианы
        auto calcMedianFunc = [&copyDataVect](size_t startIndex, size_t endIndex) -> float
        {
            double middleIndex = double(endIndex + startIndex) / 2.;

            return (copyDataVect[(size_t)floor(middleIndex)] +
                    copyDataVect[(size_t)ceil(middleIndex)]) / 2.f;
        };

        const size_t endIndex = dataSize - 1;

        // центральная медиана
        const float centerMedian = calcMedianFunc(0, endIndex);

        // вычисляем нижний квартиль
        float startMedianQ1;
        {
            size_t currentIndex = (size_t)floor(double(endIndex) / 2.);
            do
            {
                startMedianQ1 = calcMedianFunc(0, currentIndex);

                // Если квартиль совпал с медианой - значит надо делать более узкую выборку
                // бьем повторно пока выборка не "обмелеет" или не будет отличаться от медианы
                currentIndex /= 2;
            } while (startMedianQ1 == centerMedian && currentIndex > 10);
        }

        // вычисляем верхний квартиль
        float endMedianQ3;
        {
            size_t currentIndex = (size_t)ceil(double(endIndex) / 2.);
            do
            {
                endMedianQ3 = calcMedianFunc(endIndex - currentIndex, endIndex);

                // Если квартиль совпал с медианой - значит надо делать более узкую выборку
                // бьем повторно пока выборка не "обмелеет" или не будет отличаться от медианы
                currentIndex /= 2;
            } while (endMedianQ3 == centerMedian && currentIndex > 10);
        }

        // межквартильный диапазон
        float diapason = endMedianQ3 - startMedianQ1;

        // поправляем диапазон на 1.5
        diapason *= 1.5f;

        float validIntervalStart = startMedianQ1 - diapason;
        float validIntervalEnd = endMedianQ3 + diapason;

        // удаляем данные не в нашем диапазоне
        deleteInappropriateValues(data,
                                  [validIntervalStart, validIntervalEnd](const float& value) mutable
                                  {
                                      return value < validIntervalStart ||
                                          value > validIntervalEnd;
                                  });
    }
    else
    {
        // удаляем выделяющиеся данные
        deleteInappropriateValues(data, [&averageVal](const float& value) mutable
                                  {
                                      return abs(value - averageVal) > 30;
                                  });
    }
}

void ChannelDataGetter::DeleteEmptyData(std::vector<float>& data, float* averageVal) const
{
    // удаляем нули и наны
    const float averageValue = deleteInappropriateValues(data, [](const float& value) mutable
                                                         {
                                                             return abs(value) <= FLT_MIN;
                                                         });;

    if (averageVal)
        *averageVal = averageValue;
}

void ChannelDataGetter::FillChannelList(std::list<CString>& channels, bool signals)
{
    std::list<std::pair<CString, CString>> channelsList;
    get_channel_list(channelsList, (signals ? GetSignalsDir() : GetCompressedDir()).c_str(), false);
    channels.resize(channelsList.size());
    std::transform(channelsList.begin(), channelsList.end(), channels.begin(), [](const auto& name) { return name.first; });
    channels.sort(sort_text_with_numbers);
}

void ChannelDataGetter::FillChannelListWithGroupNames(std::list<std::pair<CString, CString>>& channelsWithGroupNames, bool signals)
{
    get_channel_list(channelsWithGroupNames, (signals ? GetSignalsDir() : GetCompressedDir()).c_str(), true);
    channelsWithGroupNames.sort([](const std::pair<CString, CString>& first, const std::pair<CString, CString>& second)
    {
        return sort_text_with_numbers(first.first, second.first);
    });
}

float ChannelDataGetter::deleteInappropriateValues(std::vector<float>& datavect, Callback&& callback) const
{
    datavect.erase(std::remove_if(datavect.begin(), datavect.end(), [&callback](auto val)
                                  {
                                      return callback(val);
                                  }), datavect.end());

    return std::accumulate(datavect.begin(), datavect.end(), 0.f) /
        (datavect.empty() ? 1 : datavect.size());
}

//----------------------------------------------------------------------------//
void ChannelDataGetter::calcChannelInfo(const CTime& begin, const CTime& end, bool compressionDir)
{
    const CTime tBegin = CTime(begin.GetYear(), begin.GetMonth(), begin.GetDay(), begin.GetHour(), 0, 0);
    const int offset = end.GetMinute() * 60 + end.GetSecond();
    const CTime tEnd = offset > 0 ? (end - CTimeSpan(offset - 3600)) : end;

    // директория с сигналами
    const auto sourceFilesDir = compressionDir ? GetCompressedDir() : GetSignalsDir();

    // имя файла
    CString sFileName;

    CTime tTimeNext = tBegin;
    while (tTimeNext < tEnd)
    {
        if (compressionDir)
            sFileName.Format(L"%s%d\\%02d\\sig0000.xml", sourceFilesDir.c_str(), tTimeNext.GetYear(), tTimeNext.GetMonth());
        else
        {
            if (tTimeNext.GetHour() == 0)
            {
                sFileName.Format(L"%s%d\\%02d\\%02d\\", sourceFilesDir.c_str(), tTimeNext.GetYear(), tTimeNext.GetMonth(), tTimeNext.GetDay());
                if (!std::filesystem::exists(sFileName.GetString()))
                {
                    tTimeNext += 3600 * 24;
                    continue;
                }
            }
            sFileName.Format(L"%s%d\\%02d\\%02d\\%02d\\sig0000.xml", sourceFilesDir.c_str(), tTimeNext.GetYear(), tTimeNext.GetMonth(), tTimeNext.GetDay(), tTimeNext.GetHour());
        }

        // пытаемся вытащить информацию о канале
        if (getChannelInfo(sFileName))
            return;

        tTimeNext += 3600;
    }

    // если не нашли информацию о канале вытаскиваем ее из файла с информацией
    const CString reserveFileInfo = std::filesystem::get_exe_directory().append(L"sig0000.xml").c_str();
    if (!getChannelInfo(reserveFileInfo))
    {
        const auto errorText = std::string_swprintf(
               L"Не удалось вытащить информацию о канале, возможно данных по каналу нет в интервале %s-%s,"
               L" нету исходных файлов и не подошел резервный файл, имя канала: %s\n"
               L"Резевный файл(нужен sig0000.xml с названием канала): %s",
               begin.Format(L"%d.%m %H:%M").GetString(), end.Format(L"%d.%m %H:%M").GetString(),
               m_channelInfo->GetName().GetString(), reserveFileInfo.GetString());
        throw ext::exception(std::narrow(errorText.c_str()).c_str());
    }
}

//----------------------------------------------------------------------------//
ChannelsFromFileByYear ChannelDataGetter::GetSourceFilesInfoByInterval(const CTime& Begin, const CTime& End)
{
    // перечень всех файлов которые должны быть
    ChannelsFromFileByYear filesByDate;

    const CTime tBegin = CTime(Begin.GetYear(), Begin.GetMonth(), Begin.GetDay(), Begin.GetHour(), 0, 0);
    const int offset = End.GetMinute() * 60 + End.GetSecond();
    const CTime tEnd = offset > 0 ? (End - CTimeSpan(offset - 3600)) : End;

    // директория с сигналами
    const auto sourceFilesDir = GetSignalsDir();
    // имя файла
    CString sFileName;

    CTime tTimeNext = tBegin;
    while (tTimeNext < tEnd)
    {
        if (tTimeNext.GetHour() == 0)
        {
            sFileName.Format(L"%s%d\\%02d\\%02d\\", sourceFilesDir.c_str(), tTimeNext.GetYear(), tTimeNext.GetMonth(), tTimeNext.GetDay());
            if (!std::filesystem::exists(sFileName.GetString()))
            {
                tTimeNext += 3600 * 24;
                continue;
            }
        }
        sFileName.Format(L"%s%d\\%02d\\%02d\\%02d\\sig0000.xml", sourceFilesDir.c_str(), tTimeNext.GetYear(), tTimeNext.GetMonth(), tTimeNext.GetDay(), tTimeNext.GetHour());

        xml_document hFile;
        if (hFile.load_file(sFileName, parse_default, encoding_utf8))
        {
            const xml_node hSignal = get_signal(hFile, m_channelInfo);
            if (hSignal != NULL)
            {
                ChannelFromFile files;
                files.sDataFileName = hSignal.attribute(L"data_file").value();
                files.sDescriptorFileName = hSignal.attribute(L"descriptor_file").value();
                // пытаемся вытащить информацию о файлах с данными
                filesByDate[tTimeNext.GetYear()].ChannelsFromFileByMonth[tTimeNext.GetMonth()].ChannelsFromFileByDay[tTimeNext.GetDay()].ChannelsFromFileByHour[tTimeNext.GetHour()] =
                    std::move(files);
            }
        }

        tTimeNext += 3600;
    }

    return filesByDate;
}

//----------------------------------------------------------------------------//
bool ChannelDataGetter::getChannelInfo(const CString& xmlFileName) const
{
    xml_document hFile;
    if (hFile.load_file(xmlFileName, parse_default, encoding_utf8))
    {
        const xml_node hSignal = get_signal(hFile, m_channelInfo);
        if (hSignal != NULL)
        {
            //m_channelInfo->m_sName = m_channelName;
            m_channelInfo->m_sComment = hSignal.attribute(L"comment").value();
            m_channelInfo->m_dFrequency = hSignal.attribute(L"frequency").as_double();
            m_channelInfo->m_fMinlevel = hSignal.attribute(L"minlevel").as_float();
            m_channelInfo->m_fMaxlevel = hSignal.attribute(L"maxlevel").as_float();
            m_channelInfo->m_fSense = hSignal.attribute(L"sense").as_float();
            m_channelInfo->m_fReference = hSignal.attribute(L"reference").as_float();
            m_channelInfo->m_fShift = hSignal.attribute(L"shift").as_float();
            m_channelInfo->m_sConversion = hSignal.attribute(L"conversion").value();
            m_channelInfo->m_iTypeAdc = hSignal.attribute(L"typeADC").as_int();
            m_channelInfo->m_iNumberDSP = hSignal.attribute(L"numberDSP").as_int();
            m_channelInfo->m_iChannel = hSignal.attribute(L"channel").as_int();

            GUID guid = { 0 };
            //запрашиваем CLSID компонента Список переменных
            const HRESULT hRes = CLSIDFromString(hSignal.attribute(L"id").value(), &guid);
            EXT_ASSERT(hRes == S_OK);// все оч плохо

            m_channelInfo->m_ID = guid;
            m_channelInfo->m_iType = hSignal.attribute(L"type").as_int();
            m_channelInfo->m_sGroupName = hSignal.attribute(L"groupname").value();

            return true;
        }
    }

    return false;
}

std::shared_ptr<ChannelInfo> ChannelDataGetter::getChannelInfo() const
{
    return m_channelInfo;
}


//----------------------------------------------------------------------------//
CString GetAnotherPath(const wchar_t* sSourcePath, const CString& sPath,
                       unsigned int depthLevel)
{
    ++depthLevel;
    std::list<CString> vPath;
    int iCurPos(0);
    CString sRes = sPath.Tokenize(_T("\\"), iCurPos);
    while (!sRes.IsEmpty())
    {
        vPath.push_back(sRes);
        sRes = sPath.Tokenize(_T("\\"), iCurPos);
    }

    CString sRet;
    if (vPath.size() > depthLevel)
    {
        sRet = sSourcePath;
        sRet.TrimRight(L"\\");

        std::for_each(std::prev(vPath.end(), depthLevel), vPath.end(), [&](CString &s)
        {
            sRet += L"\\";
            sRet += s;
        });
    }

    return sRet;
}

void ChannelDataGetter::DeleteAllChannelData(const CTime& Begin, const CTime& End)
{
    const CTime tBegin = CTime(Begin.GetYear(), Begin.GetMonth(), Begin.GetDay(), Begin.GetHour(), 0, 0);
    const int offset = End.GetMinute() * 60 + End.GetSecond();
    const CTime tEnd = offset > 0 ? (End - CTimeSpan(offset - 3600)) : End;

    // директория с сигналами
    auto filesDir = GetSignalsDir();
    // имя файла
    CString sFileName;

    auto remove = [&](const std::filesystem::path& filePath, bool signals)
    {
        if (std::filesystem::exists(filePath))
            std::filesystem::remove(filePath);
        else
        {
            const CString res = GetAnotherPath(filesDir.c_str(), filePath.c_str(), signals ? 4 : 2);
            if (std::filesystem::exists(res.GetString()))
                std::filesystem::remove(res.GetString());
        }
    };
    auto removeFromMainXMLAndCleanDir = [&](xml_node& hSignal)
    {
        auto parent = hSignal.parent();
        parent.remove_child(hSignal);
        if (parent.first_child() == nullptr)
        {
            std::filesystem::path mainXMLPath = sFileName.GetString();
            remove(mainXMLPath, true);
            while (std::filesystem::is_empty(mainXMLPath.parent_path()))
            {
                mainXMLPath = mainXMLPath.parent_path();
                std::filesystem::remove(mainXMLPath);
            }
        }
    };

    CTime tTimeNext = tBegin;
    while (tTimeNext < tEnd)
    {
        if (tTimeNext.GetHour() == 0)
        {
            sFileName.Format(L"%s%d\\%02d\\%02d\\", filesDir.c_str(), tTimeNext.GetYear(), tTimeNext.GetMonth(), tTimeNext.GetDay());
            if (!std::filesystem::exists(sFileName.GetString()))
            {
                tTimeNext += 3600 * 24;
                continue;
            }
        }

        sFileName.Format(L"%s%d\\%02d\\%02d\\%02d\\sig0000.xml", filesDir.c_str(), tTimeNext.GetYear(), tTimeNext.GetMonth(), tTimeNext.GetDay(), tTimeNext.GetHour());
        xml_document hFile;
        if (hFile.load_file(sFileName, parse_default, encoding_utf8))
        {
            xml_node hSignal = get_signal(hFile, m_channelInfo);
            if (hSignal != NULL)
            {
                std::filesystem::path dataFile = hSignal.attribute(L"data_file").value();
                remove(dataFile, true);
                dataFile.replace_extension(L"anp");
                remove(dataFile, true);

                remove(hSignal.attribute(L"descriptor_file").value(), true);

                removeFromMainXMLAndCleanDir(hSignal);
                hFile.save_file(sFileName, L"\t", format_default, encoding_utf8);
            }
        }

        tTimeNext += 3600;
    }

    const std::list<std::wstring> compressedFileExtensions = { L".01d", L".01h", L".01m", L".01s", L".06h", L".10m", L".10s", L".anp", L".xml"};

    filesDir = GetCompressedDir();
    tTimeNext = tBegin;
    while (tTimeNext < tEnd)
    {
        sFileName.Format(L"%s%d\\%02d\\sig0000.xml", filesDir.c_str(), tTimeNext.GetYear(), tTimeNext.GetMonth());

        xml_document hFile;
        if (hFile.load_file(sFileName, parse_default, encoding_utf8))
        {
            xml_node hSignal = get_signal(hFile, m_channelInfo);
            if (hSignal != NULL)
            {
                const std::wstring str = hSignal.attribute(L"data_file").value();
                for (auto& extension : compressedFileExtensions)
                {
                    remove(str + extension, false);
                }

                removeFromMainXMLAndCleanDir(hSignal);
                hFile.save_file(sFileName, L"\t", format_default, encoding_utf8);
            }
        }

        tTimeNext += 3600;
    }
}
//----------------------------------------------------------------------------//
void Resampling(float* pSource, long lSourceSize, float* pDestination, long lDestinationSize)
{
    if (lSourceSize > lDestinationSize)
    {
        for (long i = 0; i < lDestinationSize; i += 2)
        {
            long lIndex = (long)(round(double(i) / double(lDestinationSize) * double(lSourceSize)));
            long lIndex2 = (long)(round(double(i + 2.) / double(lDestinationSize) * double(lSourceSize)));
            if (lIndex > lSourceSize - 1)
                lIndex = lSourceSize - 1;
            if (lIndex2 > lSourceSize - 1)
                lIndex2 = lSourceSize - 1;

            float fMin(NAN), fMax(NAN);
            int iMin(lIndex), iMax(lIndex);

            bool notNANFound = false;
            for (long j = lIndex; j < lIndex2; ++j)
            {
                if (_finite(pSource[j]) == 0)
                {
                    // если нан - ничего сделать нельзя
                    fMin = fMax = NAN;
                    iMin = iMax = lIndex;
                    break;
                }
                else
                {
                    if (!notNANFound)
                    {
                        fMin = fMax = pSource[j];
                        iMin = iMax = j;
                    }
                    else
                    {
                        if (fMin > pSource[j])
                            fMin = pSource[j];
                        if (fMax < pSource[j])
                            fMax = pSource[j];

                        notNANFound = true;
                    }
                }
            }

            if (iMin > iMax)
            {
                pDestination[i] = fMax;
                pDestination[i + 1] = fMin;
                //TRACE(L"From %d to %d: %d = %f, %d = %f\n", lIndex, lIndex2, i, fMax, i + 1, fMin);
            }
            else
            {
                pDestination[i + 1] = fMax;
                pDestination[i] = fMin;
                //TRACE(L"From %d to %d: %d = %f, %d = %f\n", lIndex, lIndex2, i, fMin, i + 1, fMax);
            }
        }
    }
    else
    {
        for (int i = 0; i < lDestinationSize; ++i)
        {
            double index = double(i) / double(lDestinationSize) * double(lSourceSize);
            long lIndex = (long)(floor(index));

            double deltaIndex = index - lIndex;
            if (lIndex + 1 == lSourceSize)
                pDestination[i] = pSource[lIndex];
            else
            {
                if (abs(pSource[lIndex]) < FLT_MIN && abs(pSource[lIndex + 1]) >= FLT_MIN)
                    pDestination[i] = pSource[lIndex + 1];
                else if (abs(pSource[lIndex]) >= FLT_MIN && abs(pSource[lIndex + 1]) < FLT_MIN)
                    pDestination[i] = pSource[lIndex];
                else
                    pDestination[i] = (float)(pSource[lIndex] * (1. - deltaIndex) + pSource[lIndex + 1] * deltaIndex);
            }
        }
    }
}

//----------------------------------------------------------------------------//
void ChannelDataGetter::GetSourceChannelDataByInterval(const CTime& tStart, const CTime& tEnd,
                                                       long uiLength, float* pData)
{
    ChannelsFromFileByYear channelMap = GetSourceFilesInfoByInterval(tStart, tEnd);

    long lLength(uiLength);

    //округленное в меньшую сторону до целых секунд время начала
    //CZetTime tBegin(tStart);
    //double begin = double(tBegin.GetNanoseconds()) / 1.e9;
    //tBegin.RoundSecondsDown();
    CTime tIntervalBegin = tStart;

    double begin = 0;

    //округленное в большую сторону до целых секунд время конца
    //CZetTime tFinish(tEnd);
    //double end = 1. - double(tFinish.GetNanoseconds()) / 1.e9;
    //tFinish.RoundSecondsUp();
    CTime tIntervalEnd = tEnd;
    double end = 1;

    //время начала часовой записи
    CTime timeBegin = CTime(tIntervalBegin.GetYear(), tIntervalBegin.GetMonth(), tIntervalBegin.GetDay(), tIntervalBegin.GetHour(), 0, 0);
    CString sBegin(timeBegin.Format(L"%Y-%m-%d-%H-%M-%S"));

    //время окончания часовой записи
    int offset = tIntervalEnd.GetMinute() * 60 + tIntervalEnd.GetSecond();
    CTime timeEnd = offset > 0 ? (tIntervalEnd - CTimeSpan(offset - 3600)) : tIntervalEnd;
    CString sEnd(timeEnd.Format(L"%Y-%m-%d-%H-%M-%S"));

    //гранулярность для начального адреса, в котором может быть выделена виртуальная память
    const DWORD dwAllocationGranularity = get_allocation_granularity();

    const auto signalsDir = GetSignalsDir();

    CTime tTimeNext = timeBegin;
    CTime tTimeCurrent = tIntervalBegin;
    CTime tCurrentBegin = CTime(tStart);
    double currentBegin = begin;
    double currentEnd = 0.;
    long lPosition(0);
    while (tTimeNext < timeEnd)
    {
        //текущее время начала часовой записи
        timeBegin = tTimeNext;
        //время начала следующей часовой записи
        tTimeNext += 3600;

        if (tTimeNext > tIntervalEnd)
        {
            tTimeNext = tIntervalEnd;
            timeEnd = tIntervalEnd;
            currentEnd = end;
        }
        //CString sTimeCurrent(tTimeCurrent.Format(L"%Y-%m-%d-%H-%M-%S"));
        //CString sTimeNext(tTimeNext.Format(L"%Y-%m-%d-%H-%M-%S"));

        CString sFileName = channelMap[tTimeCurrent.GetYear()].ChannelsFromFileByMonth[tTimeCurrent.GetMonth()].ChannelsFromFileByDay[tTimeCurrent.GetDay()].ChannelsFromFileByHour[tTimeCurrent.GetHour()].sDataFileName;

        struct __stat64 st;
        int iStatRes = _tstat64(sFileName, &st);
        if (iStatRes != 0)
        {
            sFileName = GetAnotherPath(signalsDir.c_str(), sFileName, 4);
            iStatRes = _tstat64(sFileName, &st);
        }
        if (iStatRes == 0)
        {
            if (st.st_size > 0)
            {
                DWORD dwFileSize = DWORD(st.st_size / sizeof(float));
                double dFileDelta = 3600. / double(dwFileSize);
                HANDLE hFile(CreateFile(sFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    HANDLE hMap(CreateFileMapping(hFile, NULL, PAGE_READONLY, 0x00000000, 0x00000000, NULL));
                    if (hMap)
                    {
                        //смещение в файле исходных данных (которое нужно)
                        long long llFileOffset = long long((double(CTimeSpan(tCurrentBegin - timeBegin).GetTotalSeconds()) + currentBegin) / dFileDelta) * sizeof(float);
                        long long llTail = dwFileSize * sizeof(float);
                        //смещение в файле сжатых данных (приведенное к гранулярности)
                        long long llFileOffsetReal = llFileOffset / dwAllocationGranularity * dwAllocationGranularity;
                        long lDeltaFileOffset(long(llFileOffset - llFileOffsetReal));
                        //количество точек
                        double dFactor = m_channelInfo->GetFrequency() * dFileDelta;
                        long lCount = long(lLength * dFactor);
                        if (llTail - llFileOffset < (long long)lCount * sizeof(float))
                            lCount = (long)((llTail - llFileOffset) / sizeof(float));

                        unsigned long ulSize = unsigned long(sizeof(float)* lCount + lDeltaFileOffset);
                        float* pView = reinterpret_cast<float*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, DWORD(llFileOffsetReal), ulSize));
                        if (pView != NULL)
                        {
                            float* pSrcData = &pView[lDeltaFileOffset / sizeof(float)];
                            float* pDestData = pData + lPosition;

                            // проверяем что при загрузке данных не будет выхода за пределы диапазона
                            if (lPosition + lCount > uiLength)
                                lCount = uiLength - lPosition;
                            if (lCount < 0)
                                break;

                            if (dFactor == 1.)
                            {
                                memcpy_s(pDestData, size_t(sizeof(float)* lCount),
                                         pSrcData,  size_t(sizeof(float)* lCount));
                                //TRACE("Fill %d points by source\n", lCount);
                            }
                            else
                            {
                                //Прореживание исходных данных
                                if (dFactor > 1.)
                                {
                                    if (dFactor - floor(dFactor) > 0)
                                    {
                                        long lQuantity = long(double(lCount) / dFactor);
                                        if (lPosition + lQuantity > uiLength)
                                            lQuantity = uiLength - lPosition;
                                        Resampling(pSrcData, lCount, pDestData, lQuantity);
                                        //TRACE("Fill %d points by resampling\n", lQuantity);
                                    }
                                    else
                                    {
                                        long iIndex(0);
                                        for (long i = 0; i < lCount; i += long(dFactor) * 2)
                                        {
                                            float fMin, fMax;
                                            fMin = fMax = pSrcData[i];
                                            /*for (int j = 0; j < int(dFactor); ++j)
                                            {
                                                CheckMinimum(fMin, pSrcData[i + j]);
                                                CheckMaximum(fMax, pSrcData[i + j]);
                                            }*/
                                            if (lPosition + iIndex < uiLength)
                                            {
                                                pDestData[iIndex] = fMin;
                                                ++iIndex;
                                                if (lPosition + iIndex < uiLength)
                                                {
                                                    pDestData[iIndex] = fMax;
                                                    ++iIndex;
                                                }
                                            }
                                        }
                                        //TRACE("Fill %d points by decimation\n", iIndex);
                                    }
                                }
                                //Передискретизация исходных данных в более высокую частоту дискретизации
                                else
                                {
                                    long lQuantity = long(double(lCount) / dFactor);
                                    if (lPosition + lQuantity > uiLength)
                                        lQuantity = uiLength - lPosition;
                                    Resampling(pSrcData, lCount, pDestData, lQuantity);
                                    //TRACE("Fill %d points by resampling\n", lQuantity);
                                }
                            }
                            UnmapViewOfFile(pView);
                        }
                        else
                        {

                        }
                        lPosition += long(lCount / dFactor);
                        lLength -= long(lCount / dFactor);
                        CloseHandle(hMap);
                    }
                    CloseHandle(hFile);
                }
            }
        }
        else
        {
            //CString sTimeCurrent(tTimeCurrent.Format(L"%Y-%m-%d-%H-%M-%S"));
            //CString sTimeNext(tTimeNext.Format(L"%Y-%m-%d-%H-%M-%S"));
            long lCount = long(CTimeSpan(tTimeNext - tTimeCurrent).GetTotalSeconds() * m_channelInfo->GetFrequency());
            lPosition += lCount;
            lLength -= lCount;
            //TRACE("Fill %d points by empty\n", lCount);
        }
        tTimeCurrent = tTimeNext;
        tCurrentBegin = tTimeCurrent;
        currentBegin = 0.;
    }
}

void ChannelDataGetter::GetCompressedFilesMapOnInterval(CompressedLevel level, const CTime& tStart, const CTime& tEnd,
                                                        ChannelsFromFileByYear& channelMap)
{
    CString extension;
    switch (level)
    {
    case eSeconds1: extension = L".01s"; break;
    case eSeconds10: extension = L".10s"; break;
    case eMinutes1: extension = L".01m";  break;
    case eMinutes10: extension = L".10m"; break;
    case eHours1: extension = L".01h"; break;
    case eHours6: extension = L".06h";  break;
    case eDay: extension = L".01d"; break;
    default: EXT_UNREACHABLE();
    }

    const auto compressedDir = GetCompressedDir();
    CTime tTimeNext = tStart;
    while(tTimeNext < tEnd)
    {
        CString sFileName(L"");
        sFileName.Format(L"%s%d\\%02d\\sig0000.xml", compressedDir.c_str(), tTimeNext.GetYear(), tTimeNext.GetMonth());
        xml_document hFile;
        if (hFile.load_file(sFileName, parse_default, encoding_utf8))
        {
            const xml_node hSignal = get_signal(hFile, m_channelInfo);
            if (hSignal != NULL)
            {
                ChannelFromFile tempChannel;
                tempChannel.sDataFileName = hSignal.attribute(L"data_file").value() + extension;
                tempChannel.sDescriptorFileName = hSignal.attribute(L"descriptor_file").value();

                channelMap[tTimeNext.GetYear()].ChannelsFromFileByMonth[tTimeNext.GetMonth()].ChannelsFromFileByDay[tTimeNext.GetDay()].ChannelsFromFileByHour[tTimeNext.GetHour()] = std::move(tempChannel);
            }
        }
        if (tTimeNext.GetMonth() == 12)
            tTimeNext = CTime(tTimeNext.GetYear() + 1, 1, 1, 0, 0, 0);
        else
            tTimeNext = CTime(tTimeNext.GetYear(), tTimeNext.GetMonth() + 1, 1, 0, 0, 0);
    }
}

void ChannelDataGetter::GetCompressedFilesDataOnInterval(CompressedLevel level,
                                                         CTime tStart, CTime tEnd,
                                                         std::vector<float>& pData)
{
    if (tStart.GetYear() < 1970 || (tStart.GetYear() == 1970) && tStart.GetMonth() == 1)
        tStart = CTime(0);
    else
        tStart = CTime(tStart.GetYear(), tStart.GetMonth(), 1, 0, 0, 0);
    if (!((tEnd.GetDay() == 1) && (tEnd.GetHour() == 0) && (tEnd.GetMinute() == 0) && (tEnd.GetSecond() == 0)))
    {
        if (tEnd.GetMonth() == 12)
            tEnd = CTime(tEnd.GetYear() + 1, 1, 1, 0, 0, 0);
        else
            tEnd = CTime(tEnd.GetYear(), tEnd.GetMonth() + 1, 1, 0, 0, 0);
    }

    ChannelsFromFileByYear channelMap;
    GetCompressedFilesMapOnInterval(level, tStart, tEnd, channelMap);

    long compression = 1;
    switch (level)
    {
    case eSeconds1: compression = 1; break;
    case eSeconds10: compression = 10; break;
    case eMinutes1: compression = 60;  break;
    case eMinutes10: compression = 60 * 10; break;
    case eHours1: compression = 60 * 60; break;
    case eHours6: compression = 60 * 60 * 6;  break;
    case eDay: compression = 60 * 60 * 24; break;
    default: EXT_UNREACHABLE();
    }

    pData.resize(static_cast<size_t>(2 * CTimeSpan(tEnd - tStart).GetTotalSeconds() / compression), 0.f);

    //гранулярность для начального адреса, в котором может быть выделена виртуальная память
    const DWORD dwAllocationGranularity = get_allocation_granularity();
    const auto compressedDir = GetCompressedDir();

    CTime tTimeNext = tStart;
    CTime tTimeCurrent = tStart;
    long lPosition(0);
    while(tTimeNext < tEnd)
    {
        tStart = tTimeNext;
        if (tTimeNext.GetMonth() == 12)
            tTimeNext = CTime(tTimeNext.GetYear() + 1, 1, 1, 0, 0, 0);
        else
            tTimeNext = CTime(tTimeNext.GetYear(), tTimeNext.GetMonth() + 1, 1, 0, 0, 0);

        if (tTimeNext > tEnd)
        {
            tTimeNext = tEnd;
            tEnd = tEnd;
        }

        const CString& sFileName = channelMap[tTimeCurrent.GetYear()].ChannelsFromFileByMonth[tTimeCurrent.GetMonth()].ChannelsFromFileByDay[1].ChannelsFromFileByHour[0].sDataFileName;

        HANDLE hFile(CreateFile(sFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
        if (hFile == INVALID_HANDLE_VALUE)
            hFile = CreateFile(GetAnotherPath(compressedDir.c_str(), sFileName, 2), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            HANDLE hMap(CreateFileMapping( hFile, NULL, PAGE_READONLY, NULL, NULL, NULL ));
            if (hMap)
            {
                //смещение в файле сжатых данных (которое нужно)
                long long llFileOffset = long(CTimeSpan(tTimeCurrent - tStart).GetTotalSeconds() / compression) * 2 * sizeof (float);
                //смещение в файле сжатых данных (приведенное к гранулярности)
                long long llFileOffsetReal = llFileOffset / dwAllocationGranularity * dwAllocationGranularity;
                long lDeltaFileOffset(long(llFileOffset - llFileOffsetReal));
                //количество точек
                long lCount = long(CTimeSpan(tTimeNext - tTimeCurrent).GetTotalSeconds() / compression);
                unsigned long ulSize = unsigned long(2 * sizeof(float) * lCount + lDeltaFileOffset);
                float* pView = reinterpret_cast<float*>(MapViewOfFile(hMap, FILE_MAP_READ, 0, DWORD(llFileOffsetReal), ulSize));
                if (pView != NULL)
                {
                    float* pSrcData = &pView[lDeltaFileOffset / sizeof(float)];
                    float* pDestData = pData.data() + lPosition;
                    memcpy_s(pDestData, 2 * sizeof(float)* lCount, pSrcData, 2 * sizeof(float)* lCount);
                    UnmapViewOfFile(pView);
                }
                lPosition += lCount * 2;
                CloseHandle(hMap);
            }
            else
            {
                lPosition += long(CTimeSpan(tTimeNext - tTimeCurrent).GetTotalSeconds() / compression) * 2;
            }
            CloseHandle(hFile);
        }
        else
        {
            lPosition += long(CTimeSpan(tTimeNext - tTimeCurrent).GetTotalSeconds() / compression) * 2;
        }

        tTimeCurrent = tTimeNext;
    }
}
