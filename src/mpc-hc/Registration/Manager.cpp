#include "stdafx.h"
#include "Manager.h"

#include "Branding.h"
#include "Backend.h"
#include "Registry.h"

#include "SysVersion.h"

namespace Registration
{
    namespace
    {
        bool IsDelegateExecuteType(Type type)
        {
            return type == Type::UserWithDelegateExecute || type == Type::SystemWithDelegateExecute;
        }

        void TrimPath(CString& path)
        {
            path = path.TrimLeft(_T('"'));
            int i = path.Find(_T('"'));
            if (i >= 0) {
                path = path.Left(i);
            }
        }
    }

    void Register(Type type)
    {
        ASSERT(type != Type::None);
        ASSERT(type != Type::Legacy);

        const bool vistaOrLater = SysVersion::IsVistaOrLater();
        const bool systemLevel = IsSystemLevelType(type);
        const bool withDelegateExecute = SysVersion::Is7OrLater() && IsDelegateExecuteType(type);

        Backend backend(systemLevel, withDelegateExecute);

        if (systemLevel) {
            backend.CleanLegacy();
        }

        if (withDelegateExecute) {
            backend.RegisterServer();
        }

        backend.RegisterProgids();
        backend.RegisterSharedProgids();
        backend.RegisterFolderVerbs();

        if (vistaOrLater || systemLevel) {
            backend.RegisterAutoplay();
        }

        backend.RegisterApp();

        if (vistaOrLater) {
            backend.RegisterDefaultApp();
        }

        backend.Notify();
    }

    void Unregister(Type type)
    {
        ASSERT(type != Type::None);

        const bool systemLevel = IsSystemLevelType(type);

        Backend backend(systemLevel, false);

        if (systemLevel) {
            backend.CleanLegacy();
        }

        if (type == Type::Legacy) {
            return;
        }

        backend.UnregisterDefaultApp();
        backend.UnregisterApp();
        backend.UnregisterAutoplay();
        backend.UnregisterFolderVerbs();
        backend.UnregisterSharedProgids();
        backend.UnregisterProgids();
        backend.UnregisterServer();

        backend.Notify();
    }

    Type QueryCurrentUserLevelRegistration(CString& path)
    {
        Type type = Type::None;

        // TODO: extend markers
        CString server = _T("Software\\Classes\\CLSID\\") + GetDelegateExecuteClsid() + _T("\\LocalServer32");
        CString marker = _T("Software\\Classes\\") + GetProgidPrefix() + _T("url.rtp\\shell\\open\\command");

        if (Registry::GetValue(HKEY_CURRENT_USER, server, nullptr, path)) {
            type = Type::UserWithDelegateExecute;
        } else if (Registry::GetValue(HKEY_CURRENT_USER, marker, nullptr, path)) {
            type = Type::User;
        }

        TrimPath(path);

        return type;
    }

    Type QueryCurrentSystemLevelRegistration(CString& path)
    {
        Type type = Type::None;

        // TODO: extend markers
        CString server = _T("Software\\Classes\\CLSID\\") + GetDelegateExecuteClsid() + _T("\\LocalServer32");
        CString marker = _T("Software\\Classes\\") + GetProgidPrefix() + _T("url.rtp\\shell\\open\\command");
        CString legacyMarker32 = _T("Software\\Classes\\mplayerc.avi\\shell\\open\\command");
        CString legacyMarker64 = _T("Software\\Classes\\mplayerc64.avi\\shell\\open\\command");

        if (Registry::GetValue(HKEY_LOCAL_MACHINE, server, nullptr, path)) {
            type = Type::SystemWithDelegateExecute;
        } else if (Registry::GetValue(HKEY_LOCAL_MACHINE, marker, nullptr, path)) {
            type = Type::System;
        } else if (Registry::GetValue(HKEY_LOCAL_MACHINE, legacyMarker32, nullptr, path) ||
                   Registry::GetValue(HKEY_LOCAL_MACHINE, legacyMarker64, nullptr, path)) {
            type = Type::Legacy;
        }

        TrimPath(path);

        return type;
    }

    Type QueryCurrentRegistration(CString& path)
    {
        Type type = QueryCurrentUserLevelRegistration(path);

        if (type == Type::None) {
            type = QueryCurrentSystemLevelRegistration(path);
        }

        return type;
    }

    bool IsSystemLevelType(Type type)
    {
        switch (type) {
            case Type::Legacy:
            case Type::System:
            case Type::SystemWithDelegateExecute:
                return true;
        }
        return false;
    }

    bool IsUserLevelType(Type type)
    {
        switch (type) {
            case Type::User:
            case Type::UserWithDelegateExecute:
                return true;
        }
        return false;
    }
}
