#pragma once

namespace Registration
{
    namespace Registry
    {
        bool GetValue(HKEY root, const TCHAR* path, const TCHAR* name, CString& value);
        bool SetKey(HKEY root, const TCHAR* path, const TCHAR* value);
        bool SetValue(HKEY root, const TCHAR* path, const TCHAR* name, const TCHAR* value);
        bool DeleteKey(HKEY root, const TCHAR* path);
        bool DeleteKeys(HKEY root, const TCHAR* path, const TCHAR* mask);
        bool DeleteValue(HKEY root, const TCHAR* path, const TCHAR* name);
    }
}
