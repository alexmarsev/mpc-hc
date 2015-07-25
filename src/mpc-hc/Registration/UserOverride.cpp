#include "stdafx.h"
#include "UserOverride.h"

#include "Backend.h"
#include "Manager.h"

#include "PathUtils.h"

namespace Registration
{
    UserOverride::~UserOverride()
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void UserOverride::Apply(bool noRecentDocs, bool noFolderVerbs)
    {
        if (m_thread.joinable()) {
            m_thread.join();
        }

        m_thread = std::thread([ = ]() {
            CString path;
            if (QueryCurrentRegistration(path) != Type::None &&
                    PathUtils::GetProgramPath(true).CompareNoCase(path) == 0) {
                Backend::EnableNoRecentDocs(noRecentDocs);
                Backend::DisableFolderVerbs(noFolderVerbs);
            }
        });
    }
}
