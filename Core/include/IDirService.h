#pragma once

#include <string>

#include <ext/core/defines.h>

// Directory getter interface
struct IDirService
{
    virtual ~IDirService() = default;

    // Get directory of signal files
    EXT_NODISCARD virtual std::wstring GetZetSignalsDir() const = 0;
    // Get directory of compressed files
    EXT_NODISCARD virtual std::wstring GetZetCompressedDir() const = 0;
    // Get directory of installation Zetlab
    EXT_NODISCARD virtual std::wstring GetZetInstallDir() const = 0;
};