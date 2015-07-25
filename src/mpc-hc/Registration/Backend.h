#pragma once

#include "ShellResourceLoader.h"

namespace Registration
{
    class Backend final
    {
    public:

        Backend(bool systemLevel, bool delegateExecute);
        Backend(const Backend&) = delete;
        Backend& operator=(const Backend&) = delete;
        ~Backend();

        void CleanLegacy();

        void RegisterServer();
        void UnregisterServer();

        void RegisterProgids();
        void UnregisterProgids();

        void RegisterSharedProgids();
        void UnregisterSharedProgids();

        void RegisterFolderVerbs();
        void UnregisterFolderVerbs();

        void RegisterAutoplay();
        void UnregisterAutoplay();

        void RegisterApp();
        void UnregisterApp();

        void RegisterDefaultApp();
        void UnregisterDefaultApp();

        static void EnableNoRecentDocs(bool yes);
        static void DisableFolderVerbs(bool yes);

        static CString GetUserFileChoice(const CString& extension);
        static void SetUserFileChoice(const CString& extension, const CString& progid);
        static void UnsetUserFileChoice(const CString& extension);

        static CString GetUserProtocolChoice(const CString& protocol);
        static void SetUserProtocolChoice(const CString& protocol, const CString& progid);
        static void UnsetUserProtocolChoice(const CString& protocol);

        void Notify();

    private:

        CString GetStringDefault(int id);
        CString GetStringRedirect(int id);
        CString GetIconPath(int id);

        void RegisterVerbPair(const CString& progid, const CString& verbPrefix, bool withDelegate, bool withLegacy);

        const bool m_system;
        const HKEY m_root;
        const CString m_exeFile;
        const CString m_resFile;
        const CString m_exeIcon;
        const CString m_openCommand;
        const CString m_enqueueCommand;
        const CString m_clsid;
        const bool m_delegate;

        ShellResourceLoader m_resLoader;
    };
}
