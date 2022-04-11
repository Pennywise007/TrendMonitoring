#include "pch.h"

#include "DirServiceImpl.h"
#include "MonitoringTaskService/ChannelDataGetter/ChannelDataGetter.h"

#include <ext/utils/registry.h>

EXT_NODISCARD std::wstring DirServiceImpl::GetZetSignalsDir() const
{
    return ChannelDataGetter::GetSignalsDir();
}

EXT_NODISCARD std::wstring DirServiceImpl::GetZetCompressedDir() const
{
    return ChannelDataGetter::GetCompressedDir();
}

EXT_NODISCARD std::wstring DirServiceImpl::GetZetInstallDir() const
{
    const static auto installationPath = []()
    {
        const ext::registry::Key key(L"SOFTWARE\\ZET\\ZETLab");
        std::wstring location;
        EXT_DUMP_IF(!key.GetRegistryValue(L"InstallLocation", location)) << "Failed to read ZetLab installation dir";
        return location;
    }();
    return installationPath;
}
