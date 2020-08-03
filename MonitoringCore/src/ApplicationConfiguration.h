#pragma once

#include <list>
#include <memory>

#include <COM.h>
#include "Serialization/ISerializable.h"

#include "ITrendMonitoring.h"
#include "ITelegramUsersList.h"

////////////////////////////////////////////////////////////////////////////////
// Класс с настройками канала для мониторинга
class ATL_NO_VTABLE ChannelParameters
    : public COMInternalClass<ChannelParameters>
    , public SerializableObjectsCollection
    , public MonitoringChannelData
{
public:
    ChannelParameters() = default;
    ChannelParameters(const CString& initChannelName);

    // Объявляем сериализуемую коллекцию и свойства
    BEGIN_SERIALIZABLE_PROPERTIES(ChannelParameters)
        ADD_PROPERTY(channelName)
        ADD_PROPERTY(bNotify)
        ADD_PROPERTY(monitoringInterval)
        ADD_PROPERTY(allarmingValue)
    END_SERIALIZABLE_PROPERTIES();

public:
    /// <summary>Получить данные по каналу.</summary>
    const MonitoringChannelData& getMonitoringData();
    /// <summary>Получить данные по каналу.</summary>
    void setTrendChannelData(const TrendChannelData& data);
    /// <summary>Изменить имя канала.</summary>
    /// <param name="newName">Новое название.</param>
    /// <returns>Истину в случае изменения значения.</returns>
    bool changeName(const CString& newName);
    /// <summary>Изменить флаг нотификации о проблемаз канала.</summary>
    /// <param name="state">Новое состояние.</param>
    /// <returns>Истину в случае изменения значения.</returns>
    bool changeNotification(const bool state);
    /// <summary>Изменить интервал по каналу.</summary>
    /// <param name="newInterval">Новое значение интервала.</param>
    /// <returns>Истину в случае изменения значения.</returns>
    bool changeInterval(const MonitoringInterval newInterval);
    /// <summary>Изменить оповащаемое значение по каналу.</summary>
    /// <param name="newvalue">Новое значение.</param>
    /// <returns>Истину в случае изменения значения.</returns>
    bool changeAllarmingValue(const float newvalue);
    /// <summary>Сбросить запомненные данные по каналу.</summary>
    void resetChannelData();

public:
    // конец врменного интервала по которому были загружены параметры
    std::optional<CTime> m_loadingParametersIntervalEnd;
};

// Список каналов и с параметрами
typedef std::list<ChannelParameters::Ptr> ChannelParametersList;
// Итератор на канал
typedef std::list<ChannelParameters::Ptr>::iterator ChannelIt;

////////////////////////////////////////////////////////////////////////////////
// Класс с данными пользователя телеграма
class ATL_NO_VTABLE TelegramUser
    : public COMInternalClass<TelegramUser>
    , public SerializableObjectsCollection
    , public TgBot::User
{
public:
    // нужно переопределить ибо Ptr уже есть в TgBot::User и COMInternalClass
    typedef ComPtr<TelegramUser> Ptr;

    TelegramUser() = default;
    TelegramUser(const TgBot::User& pUser);

    // Объявляем сериализуемую коллекцию и свойства
    BEGIN_SERIALIZABLE_PROPERTIES(TelegramUser)
        ADD_PROPERTY(m_chatId)
        ADD_PROPERTY(m_userStatus)
        ADD_PROPERTY(m_userNote)
        // Параметры из TgBot::User
        ADD_PROPERTY(id)                // идентификатор пользователя
        ADD_PROPERTY(isBot)
        ADD_PROPERTY(firstName)
        ADD_PROPERTY(lastName)
        ADD_PROPERTY(username)
        ADD_PROPERTY(languageCode)
    END_SERIALIZABLE_PROPERTIES();

    TelegramUser& operator=(const TgBot::User& pUser);
    bool operator!=(const TgBot::User& pUser) const;
    // TODO C++20
    //auto operator<=>(const TgBot::User::Ptr&) const = default;

public:
    // идентификатор чата с пользователем
    int64_t m_chatId;
    // статус пользователя
    ITelegramUsersList::UserStatus m_userStatus = ITelegramUsersList::UserStatus::eNotAuthorized;
    // заметка о пользователе, пока не используется
    // в конфигурационном файле имеет место быть для заметок
    CString m_userNote;
};

////////////////////////////////////////////////////////////////////////////////
// Класс хранящий список пользователей телеграма
// позволяет работать со списком пользователей из нескольких потоков
class ATL_NO_VTABLE TelegramUsersList
    : public COMInternalClass<TelegramUsersList>
    , public SerializableObjectsCollection
    , public ITelegramUsersList
{
public:
    TelegramUsersList() = default;

    BEGIN_COM_MAP(TelegramUsersList)
        COM_INTERFACE_ENTRY(ITelegramUsersList)
        COM_INTERFACE_ENTRY_CHAIN(SerializableObjectsCollection)
    END_COM_MAP()

// ISerializableCollection
public:
    /// <summary>Оповещает о начале сериализации коллекции.</summary>
    /// <returns>true если коллекцию можно сериализовать.</returns>
    virtual bool onStartSerializing() override;
    /// <summary>Оповещает об окончании сериализации коллекции.</summary>
    virtual void onEndSerializing() override;

    /// <summary>Оповещает о начале десериализации, передает список сохраненных при сериализации имен объектов.</summary>
    /// <param name="objNames">Имена объектов которые были сохранены при сериализации.</param>
    /// <returns>true если коллекцию можно десериализовать.</returns>
    virtual bool onStartDeserializing(const std::list<CString>& objNames) override;
    /// <summary>Оповещает о конце десериализации коллекции.</summary>
    virtual void onEndDeserializing() override;

// ITelegramUsersList
public:
    // убедиться что пользователь существует
    void ensureExist(const TgBot::User::Ptr& pUser, const int64_t chatId) override;
    // получение статуса пользователя
    UserStatus getUserStatus(const TgBot::User::Ptr& pUser) override;
    // установка статуса пользователя
    void setUserStatus(const TgBot::User::Ptr& pUser, const UserStatus newStatus) override;
    // получить все идентификаторы чатов пользователей с определенным статусом
    std::list<int64_t> getAllChatIdsByStatus(const UserStatus userStatus) override;

private:
    // позволяет получить итератор на пользователя
    std::list<TelegramUser::Ptr>::iterator getUserIterator(const TgBot::User::Ptr& pUser);
    // получает итератор на пользователя или создает, если его нет
    std::list<TelegramUser::Ptr>::iterator getOrCreateUsertIterator(const TgBot::User::Ptr& pUser);
    // вызывается при изменении данных в списке пользователей
    void onUsersListChanged();

private:
    DECLARE_COLLECTION(TelegramUsersList);
    // список пользователей телеграма
    DECLARE_CONTAINER((std::list<TelegramUser::Ptr>) m_telegramUsers);

    // мьютекс на коллекцию, позволяет обращаться к ней из разных потоков
    std::mutex m_usersMutex;
};

////////////////////////////////////////////////////////////////////////////////
// Класс с настройками канала для мониторинга
class ATL_NO_VTABLE TelegramParameters
    : public COMInternalClass<TelegramParameters>
    , public SerializableObjectsCollection
    , public TelegramBotSettings
{
public:
    TelegramParameters() = default;

    // Объявляем сериализуемую коллекцию и свойства
    BEGIN_SERIALIZABLE_PROPERTIES(TelegramParameters)
        ADD_PROPERTY(bEnable)
        ADD_PROPERTY(sToken)
        ADD_SERIALIZABLE(m_telegramUsers)
    END_SERIALIZABLE_PROPERTIES();

    TelegramParameters& operator=(const TelegramBotSettings& botSettings);

public:
    // список пользователей телеграма
    TelegramUsersList::Ptr m_telegramUsers = TelegramUsersList::create();
};

////////////////////////////////////////////////////////////////////////////////
/*
    Класс, хранящий в себе настройки приложения

    Хранит:

    1. Информацию о каналах
        - выбранные каналы для мониторинга и выбранные интервалы мониторинга

    2. Информацию о пользователях телеграм бота
        - статус пользователя и его данные
*/
class ATL_NO_VTABLE ApplicationConfiguration
    : public COMInternalClass<ApplicationConfiguration>
    , public SerializableObjectsCollection
{
public:
    ApplicationConfiguration() = default;

    // Получение списка пользователей телеграма
    ITelegramUsersListPtr getTelegramUsers();
    // Получение настроек телеграм бота
    const TelegramBotSettings& getTelegramSettings();
    // Получение настроек телеграм бота
    void setTelegramSettings(const TelegramBotSettings& newSettings);

public:
    // Объявляем сериализуемую коллекцию
    DECLARE_COLLECTION(ApplicationConfiguration);

    // список настроек для каждого канала
    DECLARE_CONTAINER((ChannelParametersList) m_chanelParameters);

    // настройки телеграм бота
    DECLARE_SERIALIZABLE((TelegramParameters::Ptr) m_telegramParameters,
                         TelegramParameters::create());
};
