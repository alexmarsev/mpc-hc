#include "stdafx.h"
#include "AutoplayEvents.h"

#include "../shellres/resource.h"

namespace Registration
{
    void GetAutoplayEvents(const AutoplayEvent** events, size_t* count)
    {
        static const AutoplayEvent AutoplayEvents[] = {
            { _T("PlayVideoFilesOnArrival"), IDS_SH_AUTOPLAY_VIDEOFILES, _T("\"%1\"") },
            { _T("PlayMusicFilesOnArrival"), IDS_SH_AUTOPLAY_MUSICFILES, _T("\"%1\"") },
            { _T("PlayCDAudioOnArrival"),    IDS_SH_AUTOPLAY_AUDIOCD,    _T("/cd \"%1\"") },
            { _T("PlayDVDMovieOnArrival"),   IDS_SH_AUTOPLAY_DVDMOVIE,   _T("/dvd \"%1\"") },
        };

        ASSERT(events);
        ASSERT(count);
        *events = AutoplayEvents;
        *count = _countof(AutoplayEvents);
    }
}
