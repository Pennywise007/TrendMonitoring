#include "pch.h"

#include "DirServiceImpl.h"
#include "MonitoringTaskService/ChannelDataGetter/ChannelDataGetter.h"

#include <ext/utils/registry.h>

namespace {
EXT_NODISCARD ext::registry::Key get_setting_key() EXT_THROWS()
{
    return ext::registry::Key(L"SOFTWARE\\ZETLAB\\Settings", HKEY_LOCAL_MACHINE, KEY_READ | KEY_WOW64_64KEY);
}
} // namespace

EXT_NODISCARD std::wstring DirServiceImpl::GetZetSignalsDir() const
{
    std::wstring result;
    EXT_CHECK(get_setting_key().GetRegistryValue(L"DirSignal", result));
    return result;
}

EXT_NODISCARD std::wstring DirServiceImpl::GetZetCompressedDir() const
{
    std::wstring result;
    EXT_CHECK(get_setting_key().GetRegistryValue(L"DirCompressed", result));
    return result;
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
