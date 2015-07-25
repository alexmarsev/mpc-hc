#include "stdafx.h"
#include "Branding.h"

namespace Registration
{
    CString GetApplicationName()
    {
        return _T("MPC-HC");
    }

    CString GetClientName()
    {
        return _T("Media Player Classic");
    }

    CString GetProgidPrefix()
    {
#ifdef _WIN64
        return _T("mplayerc64.");
#else
        return _T("mplayerc.");
#endif
    }

    CString GetAutoplayHandlerPrefix()
    {
        return _T("MPC");
    }

    CString GetDelegateExecuteClsid()
    {
        // when changing this you also have to change LocalServer::ExecuteCommand class uuid
        return _T("{4A862C12-6F76-4532-9C78-0A60A2A4CD5C}");
    }

    CString GetShellResFile()
    {
        return _T("shellres.dll");
    }
}
