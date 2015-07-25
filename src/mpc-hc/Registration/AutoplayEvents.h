#pragma once

#include "../Registration/Branding.h"

#define Registration_AutoplayEventsLambda(...) [&](const AutoplayEvent& event, const CString& handler)

namespace Registration
{
    struct AutoplayEvent {
        const TCHAR* name;
        int friendlyName;
        const TCHAR* args;
    };

    void GetAutoplayEvents(const AutoplayEvent** events, size_t* count);

    template <typename F>
    void IterateAutoplayEvents(F&& f)
    {
        const AutoplayEvent* events;
        size_t count;

        GetAutoplayEvents(&events, &count);

        for (size_t i = 0; i < count; i++) {
            f(events[i], Registration::GetAutoplayHandlerPrefix() + events[i].name);
        }
    }
}
