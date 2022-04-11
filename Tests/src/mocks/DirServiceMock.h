#pragma once

#include <include/IDirService.h>

#include "gmock/gmock.h"

struct DirServiceMock : public IDirService
{
    MOCK_METHOD(std::wstring, GetZetSignalsDir, (), (const, override));
    MOCK_METHOD(std::wstring, GetZetCompressedDir, (), (const, override));
    MOCK_METHOD(std::wstring, GetZetInstallDir, (), (const, override));
};
