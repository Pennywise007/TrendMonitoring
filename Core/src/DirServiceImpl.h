#pragma once

#include "include/IDirService.h"

class DirServiceImpl : public IDirService
{
    // Get directory of signal files
    EXT_NODISCARD std::wstring GetZetSignalsDir() const override;
    // Get directory of compressed files
    EXT_NODISCARD std::wstring GetZetCompressedDir() const override;
    // Get directory of installation Zetlab
    EXT_NODISCARD std::wstring GetZetInstallDir() const override;
};

