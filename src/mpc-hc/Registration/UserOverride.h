#pragma once

#include <thread>

namespace Registration
{
    class UserOverride final
    {
    public:

        UserOverride() = default;
        UserOverride(const UserOverride&) = delete;
        UserOverride& operator=(const UserOverride&) = delete;
        ~UserOverride();

        void Apply(bool noRecentDocs, bool noFolderVerbs);

    private:

        std::thread m_thread;
    };
}
