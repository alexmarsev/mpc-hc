#include "stdafx.h"
#include "ShellResourceLoader.h"

#include "Branding.h"

#include "PathUtils.h"

namespace Registration
{
    ShellResourceLoader::ShellResourceLoader()
        : m_resFile(PathUtils::CombinePaths(PathUtils::GetProgramPath(false), GetShellResFile()))
    {
        ASSERT(PathUtils::Exists(m_resFile));
    }

    ShellResourceLoader::~ShellResourceLoader()
    {
        if (m_resLibrary) {
            FreeLibrary(m_resLibrary);
        }
    }

    CString ShellResourceLoader::GetString(int id, LANGID lang)
    {
        if (!m_resLibrary) {
            m_resLibrary = LoadLibraryEx(m_resFile, NULL, LOAD_LIBRARY_AS_DATAFILE);
        }

        HRSRC hBlock = FindResourceEx(m_resLibrary, RT_STRING, MAKEINTRESOURCE(id / 16 + 1), lang);

        HGLOBAL hRes = LoadResource(m_resLibrary, hBlock);

        auto pRes = reinterpret_cast<TCHAR*>(LockResource(hRes));

        if (pRes) {
            for (int i = 0; i < 16; i++) {
                if (*pRes) {
                    int length = *pRes;
                    pRes++;
                    if (i == id % 16) {
                        CString ret;
                        memcpy(ret.GetBufferSetLength(length), pRes, length * sizeof(TCHAR));
                        ret.ReleaseBuffer();
                        return ret;
                    }
                    pRes += length;
                } else {
                    pRes++;
                }
            }
        }

        // TODO: uncomment
        //ASSERT(FALSE);

        return _T("");
    }

    HICON ShellResourceLoader::GetIcon(int id, int w, int h)
    {
        if (!m_resLibrary) {
            m_resLibrary = LoadLibraryEx(m_resFile, NULL, LOAD_LIBRARY_AS_DATAFILE);
        }

        return (HICON)LoadImage(m_resLibrary, MAKEINTRESOURCE(id), IMAGE_ICON, w, h, 0);
    }
}
