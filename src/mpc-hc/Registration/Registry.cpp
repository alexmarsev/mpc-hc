#include "stdafx.h"
#include "Registry.h"

#include <shlwapi.h> // for SHDeleteKey() (Windows XP)
#include "WinapiFunc.h" // for RegDeleteTree() (not available in Windows XP)

namespace Registration
{
    namespace Registry
    {
        namespace
        {
            class Key final
            {
            public:

                Key() = default;
                ~Key() { RegCloseKey(m_key); }
                Key(const Key&) = delete;
                Key& operator=(const Key&) = delete;

                operator HKEY() { return m_key; }
                HKEY* operator&() { return &m_key; }

            private:

                HKEY m_key = NULL;
            };
        }

        bool SetKey(HKEY root, const TCHAR* path, const TCHAR* value)
        {
            return SetValue(root, path, _T(""), value) != ERROR_SUCCESS;
        }

        bool SetValue(HKEY root, const TCHAR* path, const TCHAR* name, const TCHAR* value)
        {
            Key key;
            if (RegCreateKeyEx(root, path, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
                return false;
            }

            size_t valueSize = (_tcslen(value) + 1) * sizeof(TCHAR);

            return RegSetValueEx(key, name, 0, REG_SZ, (const BYTE*)value, (DWORD)valueSize) != ERROR_SUCCESS;
        }

        bool GetValue(HKEY root, const TCHAR* path, const TCHAR* name, CString& value)
        {
            Key key;
            if (RegOpenKeyEx(root, path, 0, KEY_READ, &key) != ERROR_SUCCESS) {
                return false;
            }

            DWORD valueSize;
            if (RegQueryValueEx(key, name, 0, nullptr, nullptr, &valueSize)) {
                return false;
            }

            DWORD valueType;
            BYTE* buffer = (BYTE*)value.GetBufferSetLength(valueSize / sizeof(TCHAR) + 1);
            if (RegQueryValueEx(key, name, 0, &valueType, buffer, &valueSize) != ERROR_SUCCESS) {
                return false;
            }

            value.ReleaseBuffer();

            if (valueType != REG_SZ && valueType != REG_MULTI_SZ && valueType != REG_EXPAND_SZ) {
                value.Empty();
                return false;
            }

            return true;
        }

        bool DeleteKey(HKEY root, const TCHAR* path)
        {
            static const WinapiFunc<decltype(RegDeleteTree)>
            fnRegDeleteTree = { "Advapi32.dll", "RegDeleteTreeW" };
            return (fnRegDeleteTree ? fnRegDeleteTree(root, path) : SHDeleteKey(root, path)) != ERROR_SUCCESS;
        }

        bool DeleteKeys(HKEY root, const TCHAR* path, const TCHAR* mask)
        {
            Key key;
            if (RegOpenKeyEx(root, path, 0, KEY_READ | KEY_WRITE, &key) != ERROR_SUCCESS) {
                return false;
            }

            DWORD last;
            if (RegQueryInfoKey(key, nullptr, nullptr, nullptr, &last, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
                return false;
            }

            const DWORD maskLength = _tcslen(mask);
            if (maskLength == 0) {
                return false;
            }

            TCHAR buffer[256];
            for (int i = last; i > 0; i--) {
                DWORD bufferLength = _countof(buffer);
                if (RegEnumKeyEx(key, i, buffer, &bufferLength, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    if (maskLength <= bufferLength && _tcsnicmp(mask, buffer, maskLength) == 0) {
                        DeleteKey(key, buffer);
                    }
                }
            }

            return true;
        }

        bool DeleteValue(HKEY root, const TCHAR* path, const TCHAR* name)
        {
            Key key;
            if (RegOpenKeyEx(root, path, 0, KEY_WRITE, &key) != ERROR_SUCCESS) {
                return false;
            }

            return RegDeleteValue(key, name) != ERROR_SUCCESS;
        }
    }
}
