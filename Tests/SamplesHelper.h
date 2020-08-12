// вспомогательный файл для работы с образцами

#pragma once

#include <afxstr.h>

//****************************************************************************//
// Функция сравнения файла и файла из ресурсов
void compareWithResourceFile(const CString& verifiableFile,
                             const UINT resourceId, const CString& resourceName);
