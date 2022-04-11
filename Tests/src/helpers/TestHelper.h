﻿// Сервис для работы с сервисом мониторинга, защищает реальный конфигурационный файл

#pragma once

#include <filesystem>

#include <ext/core/singleton.h>

////////////////////////////////////////////////////////////////////////////////
// Сервис работы с мониторингов данных, используется для того чтобы у сервиса был получатель сообщений
// и он не кидался ассертами что никто не обрабатывает результаты,
// а так же для сохранения реального конфигурационного файла
class TestHelper
{
    friend ext::Singleton<TestHelper>;

public:
    TestHelper() = default;

public:
    // Сброс всех созданных объектов в начальное состояние
    void ResetAll() const;

// Пути к файлам
public:
    // получение пути к текущему использующемуся файлу конфигурации
    std::filesystem::path getConfigFilePath() const;
    // получение пути к сохаренной копии реального файла конфигурации
    std::filesystem::path getCopyConfigFilePath() const;

    // получить путь к реальному батнику с перезапуском системы
    std::filesystem::path getRestartFilePath() const;
    // получить путь к копии батника с перезапуском системы
    std::filesystem::path getCopyRestartFilePath() const;
};
