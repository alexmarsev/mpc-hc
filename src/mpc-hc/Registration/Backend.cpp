#include "stdafx.h"
#include "Backend.h"

#include "../MediaFormats/Formats.h"

#include "AutoplayEvents.h"
#include "Registry.h"

#include "PathUtils.h"
#include "SysVersion.h"

#include "../shellres/resource.h"

#include <shlobj.h>

namespace Registration
{
    namespace
    {
        const auto ClassesRoot = _T("Software\\Classes");
        const auto AutoplayRoot = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers");
        const auto UserFileChoiceRoot = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts");
        const auto UserProtocolChoiceRoot = _T("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations");
        const auto ClientsRoot = _T("Software\\Clients\\Media");
        const auto RegisteredApplicationsPath = _T("Software\\RegisteredApplications");

        template <typename S>
        CString GetProgidPath(S&& progid)
        {
            return CString(ClassesRoot) + _T('\\') + progid;
        }

        template <typename S>
        CString GetClsidPath(S&& clsid)
        {
            return CString(ClassesRoot) + _T("\\CLSID\\") + clsid;
        }

        template <typename S>
        CString GetAutoplayEventPath(S&& event)
        {
            return CString(AutoplayRoot) + _T("\\EventHandlers\\") + event;
        }

        template <typename S>
        CString GetAutoplayHandlerPath(S&& handler)
        {
            return CString(AutoplayRoot) + _T("\\Handlers\\") + handler;
        }

        template <typename S>
        CString GetUserFileChoicePath(S&& extension)
        {
            CString path = CString(UserFileChoiceRoot) + _T('\\') + extension;
            if (SysVersion::IsVistaOrLater()) {
                path += _T("\\UserChoice");
            }
            return path;
        }

        template <typename S>
        CString GetUserProtocolChoicePath(S&& protocol)
        {
            return CString(UserProtocolChoiceRoot) + _T('\\') + protocol + _T("\\UserChoice");
        }

        template <typename S1, typename S2>
        CString GetVerbPath(S1&& progid, S2&& verb)
        {
            return GetProgidPath(std::forward<S1>(progid)) + _T("\\shell\\") + verb;
        }

        template <typename S1, typename S2>
        CString GetVerbCommandPath(S1&& progid, S2&& verb)
        {
            return GetVerbPath(std::forward<S1>(progid), std::forward<S2>(verb)) + _T("\\command");
        }

        template <typename S>
        CString GetClientPath(S&& app)
        {
            return CString(ClientsRoot) + _T('\\') + app;
        }
    }

    Backend::Backend(bool systemLevel, bool delegateExecute)
        : m_system(systemLevel)
        , m_root(systemLevel ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER)
        , m_exeFile(PathUtils::GetProgramPath(true))
        , m_resFile(PathUtils::CombinePaths(PathUtils::GetProgramPath(false), GetShellResFile()))
        , m_exeIcon(m_exeFile + _T(",0"))
        , m_openCommand(_T("\"") + m_exeFile + _T("\" \"%1\""))
        , m_enqueueCommand(_T("\"") + m_exeFile + _T("\" /add \"%1\""))
        , m_clsid(GetDelegateExecuteClsid())
        , m_delegate(delegateExecute)
    {
        ASSERT(PathUtils::Exists(m_exeFile));
        ASSERT(PathUtils::Exists(m_resFile));
    }

    Backend::~Backend()
    {
    }

    void Backend::CleanLegacy()
    {
        Registry::DeleteKeys(HKEY_LOCAL_MACHINE, ClassesRoot, _T("mplayerc."));
        Registry::DeleteKeys(HKEY_LOCAL_MACHINE, ClassesRoot, _T("mplayerc64."));
        Registry::DeleteKeys(HKEY_LOCAL_MACHINE, GetVerbPath(_T("Directory"), _T("")), _T("mplayerc."));
        Registry::DeleteKeys(HKEY_LOCAL_MACHINE, GetVerbPath(_T("Directory"), _T("")), _T("mplayerc64."));

        Registry::DeleteValue(HKEY_LOCAL_MACHINE, RegisteredApplicationsPath, _T("Media Player Classic"));
        Registry::DeleteKey(HKEY_LOCAL_MACHINE, GetClientPath(_T("Media Player Classic")));

        const TCHAR* events[] = {
            _T("PlayVideoFilesOnArrival"),
            _T("PlayMusicFilesOnArrival"),
            _T("PlayCDAudioOnArrival"),
            _T("PlayDVDMovieOnArrival"),
        };
        const CString prefix = _T("MPC");
        for (size_t i = 0, n = _countof(events); i < n; i++) {
            Registry::DeleteValue(HKEY_LOCAL_MACHINE, GetAutoplayEventPath(events[i]), prefix + events[i]);
            Registry::DeleteKey(HKEY_LOCAL_MACHINE, GetAutoplayHandlerPath(prefix + events[i]));
        }
        Registry::DeleteKey(HKEY_LOCAL_MACHINE, GetProgidPath(_T("MediaPlayerClassic.Autorun")));
    }

    void Backend::RegisterServer()
    {
        ASSERT(m_delegate);
        const CString clsidPath = GetClsidPath(m_clsid);

        Registry::DeleteKey(m_root, clsidPath);
        Registry::SetKey(m_root, clsidPath + _T("\\LocalServer32"), m_exeFile);
        Registry::SetValue(m_root, clsidPath + _T("\\LocalServer32"), _T("ThreadingModel"), _T("Apartment"));
    }

    void Backend::UnregisterServer()
    {
        Registry::DeleteKey(m_root, GetClsidPath(m_clsid));
    }

    void Backend::RegisterProgids()
    {
        auto f1 = MediaFormats_FormatsLambda(...) {
            const CString progidPath = GetProgidPath(progid);

            Registry::DeleteKey(m_root, progidPath);

            Registry::SetKey(m_root, progidPath, GetStringDefault(format.friendlyName));
            Registry::SetValue(m_root, progidPath, _T("FriendlyTypeName"), GetStringRedirect(format.friendlyName));

            Registry::SetKey(m_root, progidPath + _T("\\DefaultIcon"), GetIconPath(format.icon));

            RegisterVerbPair(progid, _T(""), m_delegate, true);
            Registry::SetKey(m_root, progidPath + _T("\\shell"), _T("play"));
        };

        MediaFormats::IterateFormats(f1);

        auto f2 = MediaFormats_ProtocolsLambda(...) {
            const CString progidPath = GetProgidPath(progid);

            Registry::DeleteKey(m_root, progidPath);

            Registry::SetKey(m_root, progidPath, GetStringDefault(protocol.friendlyName));
            Registry::SetValue(m_root, progidPath, _T("FriendlyTypeName"), GetStringRedirect(protocol.friendlyName));
            Registry::SetValue(m_root, progidPath, _T("URL Protocol"), _T(""));

            Registry::SetKey(m_root, progidPath + _T("\\DefaultIcon"), GetIconPath(protocol.icon));

            Registry::SetKey(m_root, GetVerbCommandPath(progid, _T("open")), m_openCommand);
            if (m_delegate) {
                Registry::SetValue(m_root, GetVerbCommandPath(progid, _T("open")), _T("DelegateExecute"), m_clsid);
            }
        };

        MediaFormats::IterateProtocols(f2);
    }

    void Backend::UnregisterProgids()
    {
        auto f1 = MediaFormats_FormatsLambda(...) {
            Registry::DeleteKey(m_root, GetProgidPath(progid));
        };

        MediaFormats::IterateFormats(f1);

        auto f2 = MediaFormats_ProtocolsLambda(...) {
            Registry::DeleteKey(m_root, GetProgidPath(progid));
        };

        MediaFormats::IterateProtocols(f2);
    }

    void Backend::RegisterSharedProgids()
    {
        auto f1 = MediaFormats_ExtensionsLambda(...) {
            Registry::SetValue(m_root, GetProgidPath(ext) + _T("\\OpenWithProgids"), progid, _T(""));

            CString type;
            if (!Registry::GetValue(HKEY_CLASSES_ROOT, ext, _T("PerceivedType"), type)) {
                switch (format.type) {
                    case MediaFormats::Type::Audio:
                        Registry::SetValue(m_root, GetProgidPath(ext), _T("PerceivedType"), _T("audio"));
                        break;

                    case MediaFormats::Type::Video:
                        Registry::SetValue(m_root, GetProgidPath(ext), _T("PerceivedType"), _T("video"));
                        break;
                }
            }
        };

        MediaFormats::IterateExtensions(f1);

        auto f2 = MediaFormats_ProtocolSchemesLambda(...) {
            Registry::SetValue(m_root, GetProgidPath(scheme), _T("URL Protocol"), _T(""));
        };

        MediaFormats::IterateProtocolSchemes(f2);
    }

    void Backend::UnregisterSharedProgids()
    {
        auto f = MediaFormats_ExtensionsLambda(...) {
            Registry::DeleteValue(m_root, GetProgidPath(ext) + _T("\\OpenWithProgids"), progid);
        };

        MediaFormats::IterateExtensions(f);
    }

    void Backend::RegisterFolderVerbs()
    {
        UnregisterFolderVerbs();

        RegisterVerbPair(_T("Directory"), GetProgidPrefix(), m_delegate, false);
    }

    void Backend::UnregisterFolderVerbs()
    {
        const CString path = GetVerbPath(_T("Directory"), GetProgidPrefix());
        Registry::DeleteKey(m_root, path + _T("play"));
        Registry::DeleteKey(m_root, path + _T("enqueue"));
    }

    void Backend::RegisterAutoplay()
    {
        const CString progid = GetProgidPrefix() + _T("autoplay");

        Registry::DeleteKey(m_root, GetProgidPath(progid));

        auto f = Registration_AutoplayEventsLambda(...) {
            const CString command = _T("\"") + m_exeFile + _T("\" ") + event.args;

            Registry::SetKey(m_root, GetVerbPath(progid, event.name), GetStringDefault(event.friendlyName));
            Registry::SetKey(m_root, GetVerbCommandPath(progid, event.name), command);

            const CString handlerPath = GetAutoplayHandlerPath(handler);

            Registry::SetValue(m_root, handlerPath, _T("Action"), GetStringRedirect(event.friendlyName));
            Registry::SetValue(m_root, handlerPath, _T("Provider"), GetApplicationName());
            Registry::SetValue(m_root, handlerPath, _T("InvokeProgID"), progid);
            Registry::SetValue(m_root, handlerPath, _T("InvokeVerb"), event.name);
            Registry::SetValue(m_root, handlerPath, _T("DefaultIcon"), m_exeIcon);

            Registry::SetValue(m_root, GetAutoplayEventPath(event.name), handler, _T(""));
        };

        IterateAutoplayEvents(f);
    }

    void Backend::UnregisterAutoplay()
    {
        auto f = Registration_AutoplayEventsLambda(...) {
            Registry::DeleteValue(m_root, GetAutoplayEventPath(event.name), handler);
            Registry::DeleteKey(m_root, GetAutoplayHandlerPath(handler));
        };

        IterateAutoplayEvents(f);

        Registry::DeleteKey(m_root, GetProgidPath(GetProgidPrefix() + _T("autoplay")));
    }

    void Backend::RegisterApp()
    {
        const CString progid = _T("Applications\\") + PathUtils::BaseName(PathUtils::GetProgramPath(true));

        const CString progidPath = GetProgidPath(progid);

        Registry::SetValue(m_root, progidPath, _T("FriendlyAppName"), GetApplicationName());
        RegisterVerbPair(progid, _T(""), false, true);
        Registry::SetKey(m_root, progidPath + _T("\\shell"), _T("play"));

        auto f = MediaFormats_ExtensionsLambda(...) {
            Registry::SetValue(m_root, progidPath + _T("\\SupportedTypes"), ext, _T(""));
        };

        MediaFormats::IterateExtensions(f);
    }

    void Backend::UnregisterApp()
    {
        const CString progid = _T("Applications\\") + PathUtils::BaseName(PathUtils::GetProgramPath(true));

        Registry::DeleteKey(m_root, GetProgidPath(progid));
    }

    void Backend::RegisterDefaultApp()
    {
        const CString capabilitiesPath = GetClientPath(GetClientName()) + _T("\\Capabilities");

        Registry::DeleteKey(m_root, capabilitiesPath);

        Registry::SetValue(m_root, capabilitiesPath, _T("ApplicationName"), GetApplicationName());
        Registry::SetValue(m_root, capabilitiesPath, _T("ApplicationDescription"), GetStringRedirect(IDS_SH_APP_DESC));
        Registry::SetValue(m_root, capabilitiesPath, _T("ApplicationIcon"), m_exeIcon);

        const CString filePath = CString(capabilitiesPath) + _T("\\FileAssociations");

        auto f1 = MediaFormats_ExtensionsLambda(...) {
            Registry::SetValue(m_root, filePath, ext, progid);
        };

        MediaFormats::IterateExtensions(f1);

        const CString urlPath = CString(capabilitiesPath) + _T("\\URLAssociations");

        auto f2 = MediaFormats_ProtocolSchemesLambda(...) {
            Registry::SetValue(m_root, urlPath, scheme, progid);
        };

        MediaFormats::IterateProtocolSchemes(f2);

        Registry::SetValue(m_root, RegisteredApplicationsPath, GetClientName(), capabilitiesPath);
    }

    void Backend::UnregisterDefaultApp()
    {
        Registry::DeleteValue(m_root, RegisteredApplicationsPath, GetClientName());
        Registry::DeleteKey(m_root, GetClientPath(GetApplicationName()));
    }

    void Backend::EnableNoRecentDocs(bool yes)
    {
        auto yesFunction = MediaFormats_FormatsLambda(...) {
            Registry::SetValue(HKEY_CURRENT_USER, GetProgidPath(progid), _T("NoRecentDocs"), _T(""));
        };

        auto noFunction = MediaFormats_FormatsLambda(...) {
            Registry::DeleteValue(HKEY_CURRENT_USER, GetProgidPath(progid), _T("NoRecentDocs"));
        };

        if (yes) {
            MediaFormats::IterateFormats(yesFunction);
        } else {
            MediaFormats::IterateFormats(noFunction);
        }
    }

    void Backend::DisableFolderVerbs(bool yes)
    {
        const CString path = GetVerbPath(_T("Directory"), GetProgidPrefix());

        if (yes) {
            Registry::SetValue(HKEY_CURRENT_USER, path + _T("play"), _T("LegacyDisable"), _T(""));
            Registry::SetValue(HKEY_CURRENT_USER, path + _T("enqueue"), _T("LegacyDisable"), _T(""));
        } else {
            Registry::DeleteValue(HKEY_CURRENT_USER, path + _T("play"), _T("LegacyDisable"));
            Registry::DeleteValue(HKEY_CURRENT_USER, path + _T("enqueue"), _T("LegacyDisable"));
        }
    }

    CString Backend::GetUserFileChoice(const CString& extension)
    {
        ASSERT(!SysVersion::Is8OrLater());
        CString value;
        Registry::GetValue(HKEY_CURRENT_USER, GetUserFileChoicePath(extension), _T("ProgId"), value);
        return value;
    }

    void Backend::SetUserFileChoice(const CString& extension, const CString& progid)
    {
        ASSERT(!SysVersion::Is8OrLater());
        if (SysVersion::IsVistaOrLater()) {
            Registry::DeleteKey(HKEY_CURRENT_USER, GetUserFileChoicePath(extension));
        }
        Registry::SetValue(HKEY_CURRENT_USER, GetUserFileChoicePath(extension), _T("ProgId"), progid);
    }

    void Backend::UnsetUserFileChoice(const CString& extension)
    {
        ASSERT(!SysVersion::Is8OrLater());
        if (SysVersion::IsVistaOrLater()) {
            Registry::DeleteKey(HKEY_CURRENT_USER, GetUserFileChoicePath(extension));
        } else {
            Registry::DeleteValue(HKEY_CURRENT_USER, GetUserFileChoicePath(extension), _T("ProgId"));
        }
    }

    CString Backend::GetUserProtocolChoice(const CString& protocol)
    {
        ASSERT(SysVersion::IsVistaOrLater());
        ASSERT(!SysVersion::Is8OrLater());
        CString value;
        Registry::GetValue(HKEY_CURRENT_USER, GetUserProtocolChoicePath(protocol), _T("ProgId"), value);
        return value;
    }

    void Backend::SetUserProtocolChoice(const CString& protocol, const CString& progid)
    {
        ASSERT(SysVersion::IsVistaOrLater());
        ASSERT(!SysVersion::Is8OrLater());
        Registry::SetValue(HKEY_CURRENT_USER, GetUserProtocolChoicePath(protocol), _T("ProgId"), progid);
    }

    void Backend::UnsetUserProtocolChoice(const CString& protocol)
    {
        ASSERT(SysVersion::IsVistaOrLater());
        ASSERT(!SysVersion::Is8OrLater());
        Registry::DeleteKey(HKEY_CURRENT_USER, GetUserProtocolChoicePath(protocol));
    }

    void Backend::Notify()
    {
        // TODO: comment this
        Registry::DeleteKey(HKEY_CURRENT_USER, _T("Software\\Classes\\Local Settings\\MuiCache"));
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_DWORD, nullptr, nullptr);
    }

    CString Backend::GetStringDefault(int id)
    {
        return m_resLoader.GetString(id, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    }

    CString Backend::GetStringRedirect(int id)
    {
        CString res;
        res.Format(_T("@%s,-%d"), m_resFile, id);
        return res;
    }

    CString Backend::GetIconPath(int id)
    {
        CString icon;
        icon.Format(_T("%s,%d"), m_resFile, id);
        return icon;
    }

    void Backend::RegisterVerbPair(const CString& progid, const CString& verbPrefix, bool withDelegate, bool withLegacy)
    {
        CString verb;

        verb = verbPrefix + _T("enqueue");
        Registry::SetKey(m_root, GetVerbPath(progid, verb), GetStringDefault(IDS_SH_ENQUEUE));
        Registry::SetValue(m_root, GetVerbPath(progid, verb), _T("MUIVerb"), GetStringRedirect(IDS_SH_ENQUEUE));
        Registry::SetValue(m_root, GetVerbPath(progid, verb), _T("MultiSelectModel"), _T("Player"));
        Registry::SetValue(m_root, GetVerbPath(progid, verb), _T("Icon"), m_exeIcon);
        Registry::SetKey(m_root, GetVerbCommandPath(progid, verb), m_enqueueCommand);
        if (withDelegate) {
            Registry::SetValue(m_root, GetVerbCommandPath(progid, verb), _T("DelegateExecute"), m_clsid);
        }

        verb = verbPrefix + _T("play");
        Registry::SetKey(m_root, GetVerbPath(progid, verb), GetStringDefault(IDS_SH_PLAY));
        Registry::SetValue(m_root, GetVerbPath(progid, verb), _T("MUIVerb"), GetStringRedirect(IDS_SH_PLAY));
        Registry::SetValue(m_root, GetVerbPath(progid, verb), _T("MultiSelectModel"), _T("Player"));
        Registry::SetValue(m_root, GetVerbPath(progid, verb), _T("Icon"), m_exeIcon);
        Registry::SetKey(m_root, GetVerbCommandPath(progid, verb), m_openCommand);
        if (withDelegate) {
            Registry::SetValue(m_root, GetVerbCommandPath(progid, verb), _T("DelegateExecute"), m_clsid);
        }

        if (withLegacy) {
            verb = verbPrefix + _T("open");
            Registry::SetValue(m_root, GetVerbPath(progid, verb), _T("LegacyDisable"), _T(""));
            Registry::SetKey(m_root, GetVerbCommandPath(progid, verb), m_openCommand);
        }
    }
}
