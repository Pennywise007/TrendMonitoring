#pragma once

// Лёгкий хидер с утилитами для работы других объектов, в частности для упрощения работы тестов

#include <afxstr.h>

// имя конфигурационного файла с настройками
const CString kConfigFileName(L"AppConfig.xml");

// имя исполняемого файла для перезапуска системы
const CString kRestartSystemFileName(L"restart.bat");
