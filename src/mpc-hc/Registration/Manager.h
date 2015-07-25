#pragma once

namespace Registration
{
    enum class Type {
        None,
        Legacy,
        System,
        User,
        SystemWithDelegateExecute,
        UserWithDelegateExecute,
    };

    void Register(Type type);
    void Unregister(Type type);

    Type QueryCurrentUserLevelRegistration(CString& path);
    Type QueryCurrentSystemLevelRegistration(CString& path);
    Type QueryCurrentRegistration(CString& path);

    bool IsSystemLevelType(Type type);
    bool IsUserLevelType(Type type);
}
